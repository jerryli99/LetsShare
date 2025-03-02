#include "crypto.h"
#include <QDebug>
#include <openssl/err.h>

Crypto::Crypto(QObject *parent) : QObject(parent)
{
    encryptCtx = EVP_CIPHER_CTX_new();
    decryptCtx = EVP_CIPHER_CTX_new();
}

Crypto::~Crypto()
{
    EVP_CIPHER_CTX_free(encryptCtx);
    EVP_CIPHER_CTX_free(decryptCtx);
}

QByteArray Crypto::encryptAESKey(const QByteArray &aesKey, const QString &rsaPublicKeyPath)
{
    FILE *rsaPublicKeyFile = fopen(rsaPublicKeyPath.toStdString().c_str(), "rb");
    if (!rsaPublicKeyFile) {
        qDebug() << "Failed to open RSA public key file.";
        return QByteArray();
    }

    EVP_PKEY *pkey = PEM_read_PUBKEY(rsaPublicKeyFile, nullptr, nullptr, nullptr);
    fclose(rsaPublicKeyFile);

    if (!pkey) {
        qDebug() << "Failed to read RSA public key.";
        return QByteArray();
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx || EVP_PKEY_encrypt_init(ctx) <= 0) {
        qDebug() << "Failed to initialize RSA encryption.";
        EVP_PKEY_free(pkey);
        return QByteArray();
    }

    size_t encryptedLen;
    if (EVP_PKEY_encrypt(ctx, nullptr, &encryptedLen, reinterpret_cast<const unsigned char*>(aesKey.constData()), aesKey.size()) <= 0) {
        qDebug() << "Failed to calculate encrypted AES key length.";
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }

    QByteArray encryptedAESKey(encryptedLen, 0);
    if (EVP_PKEY_encrypt(ctx, reinterpret_cast<unsigned char*>(encryptedAESKey.data()), &encryptedLen, reinterpret_cast<const unsigned char*>(aesKey.constData()), aesKey.size()) <= 0) {
        qDebug() << "Failed to encrypt AES key.";
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return encryptedAESKey;
}

QByteArray Crypto::decryptAESKey(const QByteArray &encryptedAESKey, const QString &rsaPrivateKeyPath)
{
    FILE *rsaPrivateKeyFile = fopen(rsaPrivateKeyPath.toStdString().c_str(), "rb");
    if (!rsaPrivateKeyFile) {
        qDebug() << "Failed to open RSA private key file.";
        return QByteArray();
    }

    EVP_PKEY *pkey = PEM_read_PrivateKey(rsaPrivateKeyFile, nullptr, nullptr, nullptr);
    fclose(rsaPrivateKeyFile);

    if (!pkey) {
        qDebug() << "Failed to read RSA private key.";
        return QByteArray();
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx || EVP_PKEY_decrypt_init(ctx) <= 0) {
        qDebug() << "Failed to initialize RSA decryption.";
        EVP_PKEY_free(pkey);
        return QByteArray();
    }

    size_t decryptedLen;
    if (EVP_PKEY_decrypt(ctx, nullptr, &decryptedLen, reinterpret_cast<const unsigned char*>(encryptedAESKey.constData()), encryptedAESKey.size()) <= 0) {
        qDebug() << "Failed to calculate decrypted AES key length.";
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }

    QByteArray aesKey(decryptedLen, 0);
    if (EVP_PKEY_decrypt(ctx, reinterpret_cast<unsigned char*>(aesKey.data()), &decryptedLen, reinterpret_cast<const unsigned char*>(encryptedAESKey.constData()), encryptedAESKey.size()) <= 0) {
        qDebug() << "Failed to decrypt AES key.";
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return aesKey;
}

QByteArray Crypto::encryptChunk(const QByteArray &chunk, const QByteArray &aesKey)
{
    int len;
    QByteArray encryptedChunk(chunk.size() + EVP_MAX_BLOCK_LENGTH, 0);

    if (EVP_EncryptInit_ex(encryptCtx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>(aesKey.constData()), nullptr) <= 0 ||
        EVP_EncryptUpdate(encryptCtx, reinterpret_cast<unsigned char*>(encryptedChunk.data()), &len, reinterpret_cast<const unsigned char*>(chunk.constData()), chunk.size()) <= 0) {
        qDebug() << "Failed to encrypt chunk.";
        return QByteArray();
    }

    encryptedChunk.resize(len);
    return encryptedChunk;
}

QByteArray Crypto::decryptChunk(const QByteArray &encryptedChunk, const QByteArray &aesKey)
{
    int len;
    QByteArray decryptedChunk(encryptedChunk.size() + EVP_MAX_BLOCK_LENGTH, 0);

    if (EVP_DecryptInit_ex(decryptCtx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>(aesKey.constData()), nullptr) <= 0 ||
        EVP_DecryptUpdate(decryptCtx, reinterpret_cast<unsigned char*>(decryptedChunk.data()), &len, reinterpret_cast<const unsigned char*>(encryptedChunk.constData()), encryptedChunk.size()) <= 0) {
        qDebug() << "Failed to decrypt chunk.";
        return QByteArray();
    }

    decryptedChunk.resize(len);
    return decryptedChunk;
}
