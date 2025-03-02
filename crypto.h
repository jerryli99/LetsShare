#ifndef CRYPTO_H
#define CRYPTO_H

#include <QObject>
#include <QByteArray>
#include <openssl/evp.h>
#include <openssl/pem.h>

class Crypto : public QObject
{
    Q_OBJECT

public:
    explicit Crypto(QObject *parent = nullptr);
    ~Crypto();

    QByteArray encryptAESKey(const QByteArray &aesKey, const QString &rsaPublicKeyPath);
    QByteArray decryptAESKey(const QByteArray &encryptedAESKey, const QString &rsaPrivateKeyPath);

    QByteArray encryptChunk(const QByteArray &chunk, const QByteArray &aesKey);
    QByteArray decryptChunk(const QByteArray &encryptedChunk, const QByteArray &aesKey);

private:
    EVP_CIPHER_CTX *encryptCtx;
    EVP_CIPHER_CTX *decryptCtx;
};

#endif // CRYPTO_H
