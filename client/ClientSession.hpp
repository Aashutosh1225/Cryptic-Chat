#pragma once

#include "../network/Connection.hpp"
#include "../concurrency/MessageQueue.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

// ClientSession<SocketT>: the network-thread half of the client.
//
// ChatWindow (Phase 11) was deliberately built with zero dependency on
// Connection -- it only knows about a MessageQueue<std::string>& (for
// incoming messages, drained via tryPop() each render frame) and a
// std::function<void(const std::string&)> set via setOnSend() (for
// outgoing messages). ClientSession is the piece that fulfills both
// sides of that contract using a real Connection<SocketT>, so
// client_main.cpp's job reduces to:
//
//   ClientSession<Socket> session(std::move(socket), incomingQueue);
//   if (!session.connectAndAuthenticate(Connection<Socket>::Role::Client, "LOGIN", user, pass)) { ... }
//   session.start();                       // spawns the receive-loop thread
//   chatWindow.setOnSend([&session](const std::string& text) {
//       session.send(text);
//   });
//   ... run ChatWindow's render loop ...
//   session.stop();
//
// Deliberately independent of ChatWindow/SFML: this class only needs
// Connection (Phase 12), MessageQueue (Phase 9), and a transport
// (SocketT) -- so it can be built and unit-tested with LoopbackSocket on
// Linux, the same way Server<SocketT> was in Phase 13, without needing a
// display or SFML installed at all.
template <typename SocketT>
class ClientSession {
public:
    // Takes ownership of an already-connected socket (NOT yet
    // handshaken -- that happens inside connectAndAuthenticate()) and a
    // reference to the queue incoming broadcast messages should be
    // pushed into. The queue is owned by the caller (client_main.cpp,
    // ultimately by whatever owns ChatWindow) and must outlive this
    // ClientSession.
    ClientSession(SocketT socket, concurrency::MessageQueue<std::string>& incomingQueue)
        : connection_(std::make_unique<Connection<SocketT>>(std::move(socket))),
          incomingQueue_(incomingQueue) {}

    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;

    ~ClientSession() { stop(); }

    // Runs the RSA/AES handshake, then sends exactly one auth control
    // message ("LOGIN <user> <pass>" or "REGISTER <user> <pass>",
    // matching Server::authenticate()'s protocol from Phase 13) and
    // waits for the "OK ..."/"ERR ..." reply. Returns true only on "OK
    // ...", in which case outServerReply is set to the OK line (mainly
    // for logging/status display) and the session is ready for
    // start()/send(). On any failure, outServerReply holds whatever
    // diagnostic is available (the ERR line, or a synthetic message if
    // the handshake itself failed) and the session should be discarded
    // -- same "treat as dead, don't retry on this object" contract as
    // Connection::performHandshake().
    bool connectAndAuthenticate(typename Connection<SocketT>::Role role,
                                 const std::string& command,
                                 const std::string& username,
                                 const std::string& password,
                                 std::string& outServerReply) {
        if (!connection_->performHandshake(role)) {
            outServerReply = "ERR handshake failed";
            return false;
        }

        if (!connection_->sendMessage(0, command + " " + username + " " + password)) {
            outServerReply = "ERR failed to send auth request";
            return false;
        }

        Message reply;
        if (!connection_->receiveMessage(reply, outServerReply)) {
            outServerReply = "ERR no reply from server (connection closed)";
            return false;
        }

        if (outServerReply.rfind("OK", 0) != 0) {
            return false;   // outServerReply already holds the server's ERR text
        }

        authenticated_ = true;
        return true;
    }

    // Spawns the dedicated receive-loop thread: blocks on
    // Connection::receiveMessage() in a loop, pushing each incoming
    // plaintext into incomingQueue_ for the render thread to drain via
    // tryPop(). Must only be called once, after a successful
    // connectAndAuthenticate(). This is the "network thread separate
    // from the render loop" the project spec calls for -- ChatWindow's
    // render loop never blocks on socket I/O, it only ever does a
    // non-blocking tryPop() each frame.
    void start() {
        if (!authenticated_ || running_.load()) return;
        running_.store(true);
        receiveThread_ = std::thread([this]() { receiveLoop(); });
    }

    // Signals the receive loop to stop and joins it. Safe to call
    // multiple times (including implicitly via the destructor) and safe
    // to call even if start() was never called.
    void stop() {
        running_.store(false);
        if (connection_) {
            // There's no clean cross-platform way to interrupt a thread
            // blocked in a blocking socket read from another thread, so
            // this closes the underlying socket instead (Connection::
            // close(), added in this phase specifically for this use)
            // -- the receive thread's blocked receiveMessage() call
            // sees the socket error out and returns false on its own,
            // the same "close, don't forcibly interrupt" pattern
            // MessageQueue::shutdown() (Phase 9) uses for condition
            // variables. connection_ itself is only ever reassigned/
            // destroyed AFTER the thread below is joined, so there's no
            // race on the unique_ptr -- only on the socket it points
            // to, which is exactly what close() is designed to handle
            // being called concurrently with an in-progress receive.
            connection_->close();
        }
        if (receiveThread_.joinable()) {
            receiveThread_.join();
        }
    }

    // Encrypts and sends a chat message using this session's
    // Connection. This is what client_main.cpp wires into
    // ChatWindow::setOnSend(). Returns false if the session isn't
    // authenticated yet or the send failed (e.g. connection already
    // closed) -- the caller decides how to surface that (e.g. append a
    // "message failed to send" line to the chat log).
    bool send(const std::string& plaintext) {
        if (!authenticated_ || !connection_) return false;
        return connection_->sendMessage(0, plaintext);
    }

    bool isRunning() const { return running_.load(); }

private:
    std::unique_ptr<Connection<SocketT>> connection_;
    concurrency::MessageQueue<std::string>& incomingQueue_;
    std::thread receiveThread_;
    std::atomic<bool> running_{false};
    bool authenticated_ = false;

    void receiveLoop() {
        Message message;
        std::string plaintext;
        while (running_.load() && connection_ && connection_->receiveMessage(message, plaintext)) {
            incomingQueue_.push(plaintext);
        }
        running_.store(false);
    }
};
