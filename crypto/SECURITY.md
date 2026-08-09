# Cryptography integration notes

The application uses OpenSSL-backed AES-256-GCM for chat payloads,
RSA-3072-OAEP with SHA-256 for session-key transport, and
PBKDF2-HMAC-SHA-256 with 600,000 iterations and a fresh 16-byte salt for
password storage.

`crypto/src` is an imported, standalone code fragment rather than a buildable
part of Cryptic-Chat: it depends on absent `cipherchat/...` headers. It must
not be added to the build unchanged because it contains a fixed session IV for
GCM and non-production XOR/pass-through fallbacks. GCM requires a fresh nonce
for each encryption under a key; `Cipher.cpp` already generates a fresh random
12-byte nonce and packages it with each ciphertext.

The integrated implementation binds each chat message's sender ID and timestamp
as AES-GCM associated data. They remain readable for message framing but any
modification now fails authentication.

This is transport encryption, not end-to-end identity authentication: public
keys are exchanged without a certificate, fingerprint, or signature. An active
network attacker can therefore still mount a man-in-the-middle attack. Deploy
behind TLS or add authenticated server public-key pinning before describing the
application as end-to-end secure.
