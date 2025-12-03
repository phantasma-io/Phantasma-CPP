#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Include crypto API adaptors immediately after including PhantasmaAPI.h"
#endif

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <cstring>
#include <cerrno>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace phantasma {

inline void Ed25519_PublicKeyFromSeed(uint8_t* output, int outputLength, const uint8_t* seed, int seedLength)
{
	if (!output || outputLength < 32 || !seed || seedLength != 32)
	{
		return;
	}
	EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, seedLength);
	if (!pkey)
	{
		return;
	}
	size_t publen = 32;
	EVP_PKEY_get_raw_public_key(pkey, output, &publen);
	EVP_PKEY_free(pkey);
}

inline void Ed25519_PrivateKeyFromSeed(uint8_t* output, int outputLength, const uint8_t* seed, int seedLength)
{
	if (!output || outputLength < 64 || !seed || seedLength != 32)
	{
		return;
	}
	memcpy(output, seed, 32);
	Ed25519_PublicKeyFromSeed(output + 32, outputLength - 32, seed, seedLength);
}

inline uint64_t Ed25519_SignDetached(uint8_t* output, int outputLength, const uint8_t* message, int messageLength, const uint8_t* privateKey, int privateKeyLength)
{
	if (!output || outputLength < 64 || !message || messageLength < 0 || !privateKey || privateKeyLength < 32)
	{
		return 0;
	}
	const uint8_t* seed = privateKey;
	EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, 32);
	if (!pkey)
	{
		return 0;
	}
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	size_t siglen = (size_t)outputLength;
	if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, pkey) != 1 ||
		EVP_DigestSign(ctx, output, &siglen, message, (size_t)messageLength) != 1)
	{
		siglen = 0;
	}
	EVP_MD_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	return siglen;
}

inline uint64_t Ed25519_SignAttached(uint8_t* output, int outputLength, const uint8_t* message, int messageLength, const uint8_t* privateKey, int privateKeyLength)
{
	// Format matches libsodium: [signature||message]
	if (!output || outputLength < 64 + messageLength || !message || messageLength < 0 || !privateKey || privateKeyLength < 32)
	{
		return 0;
	}
	const uint64_t siglen = Ed25519_SignDetached(output, outputLength, message, messageLength, privateKey, privateKeyLength);
	if (siglen == 0)
	{
		return 0;
	}
	if (messageLength > 0)
	{
		memcpy(output + siglen, message, (size_t)messageLength);
	}
	return siglen + (uint64_t)messageLength;
}

inline bool Ed25519_ValidateDetached(const uint8_t* signature, int signatureLength, const uint8_t* message, int messageLength, const uint8_t* publicKey, int publicKeyLength)
{
	if (!signature || signatureLength != 64 || !message || messageLength < 0 || !publicKey || publicKeyLength != 32)
	{
		return false;
	}
	EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, publicKey, 32);
	if (!pkey)
	{
		return false;
	}
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	const bool ok = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1 &&
		EVP_DigestVerify(ctx, signature, (size_t)signatureLength, message, (size_t)messageLength) == 1;
	EVP_MD_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	return ok;
}

inline bool Ed25519_ValidateAttached(const uint8_t* signedMessage, int signedMessageLength, const uint8_t* publicKey, int publicKeyLength)
{
	if (!signedMessage || signedMessageLength < 64 || !publicKey || publicKeyLength != 32)
	{
		return false;
	}
	const uint8_t* signature = signedMessage;
	const uint8_t* message = signedMessage + 64;
	const int messageLength = signedMessageLength - 64;
	return Ed25519_ValidateDetached(signature, 64, message, messageLength, publicKey, publicKeyLength);
}

inline int Phantasma_LockMemory(void* pointer, size_t size)
{
	if (!pointer || size == 0)
	{
		return 0;
	}
#if defined(_WIN32)
	return VirtualLock(pointer, size) ? 0 : -1;
#else
	return mlock(pointer, size);
#endif
}

inline int Phantasma_UnlockMemory(void* pointer, size_t size)
{
	if (!pointer || size == 0)
	{
		return 0;
	}
#if defined(_WIN32)
	return VirtualUnlock(pointer, size) ? 0 : -1;
#else
	return munlock(pointer, size);
#endif
}

#if defined(OPENSSL_SECURE_MEMORY)
#define PHANTASMA_SECURE_ALLOC(size)         OPENSSL_secure_malloc(size)
#define PHANTASMA_SECURE_FREE(ptr)           OPENSSL_secure_free(ptr)
#define PHANTASMA_SECURE_NOACCESS(ptr, sz)   OPENSSL_secure_clear_free(ptr, sz)
#define PHANTASMA_SECURE_READONLY(ptr, size) ((void)(ptr),(void)(size),0)
#define PHANTASMA_SECURE_READWRITE(ptr,size) ((void)(ptr),(void)(size),0)
#else
#define PHANTASMA_SECURE_ALLOC(size)         OPENSSL_malloc(size)
#define PHANTASMA_SECURE_FREE(ptr)           OPENSSL_free(ptr)
#define PHANTASMA_SECURE_NOACCESS(ptr, sz)   ((void)(ptr),(void)(sz),0)
#define PHANTASMA_SECURE_READONLY(ptr, size) ((void)(ptr),(void)(size),0)
#define PHANTASMA_SECURE_READWRITE(ptr,size) ((void)(ptr),(void)(size),0)
#endif

#define PHANTASMA_RANDOMBYTES(buffer, size) RAND_bytes((unsigned char*)(buffer), (int)(size))
#define PHANTASMA_WIPEMEM(buffer, size)     OPENSSL_cleanse((buffer), (size))

#define PHANTASMA_LOCKMEM(pointer, size)    Phantasma_LockMemory((void*)(pointer), (size_t)(size))
#define PHANTASMA_UNLOCKMEM(pointer, size)  Phantasma_UnlockMemory((void*)(pointer), (size_t)(size))

#define PHANTASMA_Ed25519_PublicKeyFromSeed(output, outputLength, seed, seedLength)                                        \
                  Ed25519_PublicKeyFromSeed(output, outputLength, seed, seedLength)
#define PHANTASMA_Ed25519_PrivateKeyFromSeed(output, outputLength, seed, seedLength)                                       \
                  Ed25519_PrivateKeyFromSeed(output, outputLength, seed, seedLength)
#define PHANTASMA_Ed25519_SignAttached(output, outputLength, message, messageLength, privateKey, privateKeyLength)         \
                  Ed25519_SignAttached(output, outputLength, message, messageLength, privateKey, privateKeyLength)
#define PHANTASMA_Ed25519_SignDetached(output, outputLength, message, messageLength, privateKey, privateKeyLength)         \
                  Ed25519_SignDetached(output, outputLength, message, messageLength, privateKey, privateKeyLength)
#define PHANTASMA_Ed25519_ValidateAttached(message, messageLength, publicKey, publicKeyLength)                             \
                  Ed25519_ValidateAttached(message, messageLength, publicKey, publicKeyLength)
#define PHANTASMA_Ed25519_ValidateDetached(signature, signatureLength, message, messageLength, publicKey, publicKeyLength) \
                  Ed25519_ValidateDetached(signature, signatureLength, message, messageLength, publicKey, publicKeyLength)

inline void Phantasma_SHA256(Byte* output, int outputSize, const Byte* input, int inputSize)
{
	(void)outputSize;
	::SHA256(reinterpret_cast<const unsigned char*>(input), (size_t)inputSize, reinterpret_cast<unsigned char*>(output));
}
#define PHANTASMA_SHA256(output, outputSize, input, inputSize) ::phantasma::Phantasma_SHA256((Byte*)(output), (int)(outputSize), (const Byte*)(input), (int)(inputSize))

}
