#pragma once

#if !defined(ARCADIA_BUILD)
#include <Common/config.h>
#endif

#if USE_SSL
#include <Core/Types.h>
#include <openssl/evp.h>

namespace DB
{
class ReadBuffer;
class WriteBuffer;

namespace FileEncryption
{

/// Initialization vector. Its size is always 16 bytes.
class InitVector
{
public:
    static constexpr const size_t kSize = 16;

    InitVector() = default;
    explicit InitVector(const UInt128 & counter_) { set(counter_); }

    void set(const UInt128 & counter_) { counter = counter_; }
    UInt128 get() const { return counter; }

    void read(ReadBuffer & in);
    void write(WriteBuffer & out) const;

    /// Write 16 bytes of the counter to a string in big endian order.
    /// We need big endian because the used cipher algorithms treat an initialization vector as a counter in big endian.
    String toString() const;

    /// Converts a string of 16 bytes length in big endian order to a counter.
    static InitVector fromString(const String & str_);

    /// Adds a specified offset to the counter.
    InitVector & operator++() { ++counter; return *this; }
    InitVector operator++(int) { InitVector res = *this; ++counter; return res; }
    InitVector & operator+=(size_t offset) { counter += offset; return *this; }
    InitVector operator+(size_t offset) const { InitVector res = *this; return res += offset; }

    /// Generates a random initialization vector.
    static InitVector random();

private:
    UInt128 counter = 0;
};


/// Encrypts or decrypts data.
class Encryptor
{
public:
    /// The `key` should have length 128 or 192 or 256.
    /// According to the key's length aes_128_ctr or aes_192_ctr or aes_256_ctr will be used for encryption.
    /// We chose to use CTR cipther algorithms because they have the following features which are important for us:
    /// - No right padding, so we can append encrypted files without deciphering;
    /// - One byte is always ciphered as one byte, so we get random access to encrypted files easily.
    Encryptor(const String & key_, const InitVector & iv_);

    /// Sets the current position in the data stream from the very beginning of data.
    /// It affects how the data will be encrypted or decrypted because
    /// the initialization vector is increased by an index of the current block
    /// and the index of the current block is calculated from this offset.
    void setOffset(size_t offset_) { offset = offset_; }

    /// Encrypts some data.
    /// Also the function moves `offset` by `size` (for successive encryptions).
    void encrypt(const char * data, size_t size, WriteBuffer & out);

    /// Decrypts some data.
    /// The used cipher algorithms generate the same number of bytes in output as they were in input,
    /// so the function always writes `size` bytes of the plaintext to `out`.
    /// Also the function moves `offset` by `size` (for successive decryptions).
    void decrypt(const char * data, size_t size, char * out);

private:
    const String key;
    const InitVector init_vector;
    const EVP_CIPHER * evp_cipher;

    /// The current position in the data stream from the very beginning of data.
    size_t offset = 0;
};


/// Checks whether a passed key length is supported, i.e.
/// whether its length is 128 or 192 or 256 bits (16 or 24 or 32 bytes).
bool isKeyLengthSupported(size_t key_length);

}
}

#endif
