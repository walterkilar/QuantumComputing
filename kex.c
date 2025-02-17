/****************************************************************************************
* LatticeCrypto: an efficient post-quantum Ring-Learning With Errors cryptography library
*
*    Copyright (c) Microsoft Corporation. All rights reserved.
*
*
* Abstract: Ring-LWE key exchange
*           The implementation is based on the instantiation of Peikert's key exchange [1]
*           due to Alkim, Ducas, Poppelmann and Schwabe [2].
*
* [1] C. Peikert, "Lattice cryptography for the internet", in Post-Quantum Cryptography - 
*     6th International Workshop (PQCrypto 2014), LNCS 8772, pp. 197-219. Springer, 2014.
* [2] E. Alkim, L. Ducas, T. Pöppelmann and P. Schwabe, "Post-quantum key exchange - a new 
*     hope", IACR Cryptology ePrint Archive, Report 2015/1092, 2015.
*
******************************************************************************************/ 

#include "LatticeCrypto_priv.h"
#include <malloc.h>

extern const int32_t psi_rev_ntt1024_12289[1024];           
extern const int32_t omegainv_rev_ntt1024_12289[1024];
extern const int32_t omegainv10N_rev_ntt1024_12289;
extern const int32_t Ninv11_ntt1024_12289;

/*
 * @param clear_words Clears memory
*/
__inline void clear_words(void* mem, digit_t nwords)
{ 
  
    unsigned int i;
    volatile digit_t *v = mem; 

    for (i = 0; i < nwords; i++) {
        v[i] = 0;
    }
}

/*
 * @param LatticeCrypto_initialize Initialize structure pLatticeCrypto with user-provided functions: RandomBytesFunction, ExtendableOutputFunction and StreamOutputFunction.
*/
CRYPTO_STATUS LatticeCrypto_initialize(PLatticeCryptoStruct pLatticeCrypto, RandomBytes RandomBytesFunction, ExtendableOutput ExtendableOutputFunction, StreamOutput StreamOutputFunction)
{ 

    pLatticeCrypto->RandomBytesFunction = RandomBytesFunction;
    pLatticeCrypto->ExtendableOutputFunction = ExtendableOutputFunction;
    pLatticeCrypto->StreamOutputFunction = StreamOutputFunction;

    return CRYPTO_SUCCESS;
}

/*
 * @param LatticeCrypto_allocate Dynamically allocates memory for LatticeCrypto structure.  
*/
PLatticeCryptoStruct LatticeCrypto_allocate()
{ 
 
    PLatticeCryptoStruct LatticeCrypto = NULL;

    LatticeCrypto = (PLatticeCryptoStruct)calloc(1, sizeof(LatticeCryptoStruct));

    if (LatticeCrypto == NULL) {
        return NULL;
    }
    return LatticeCrypto;
}

/*
 * @param LatticeCrypto_get_error_message Outputs error or success message for given CRYPTO_STATUS  
*/
const char* LatticeCrypto_get_error_message(CRYPTO_STATUS Status)
{  
    struct error_mapping {
        unsigned int index;
        char*        string;
    } mapping[CRYPTO_STATUS_TYPE_SIZE] = {
        {CRYPTO_SUCCESS, CRYPTO_MSG_SUCCESS},
        {CRYPTO_ERROR, CRYPTO_MSG_ERROR},
        {CRYPTO_ERROR_DURING_TEST, CRYPTO_MSG_ERROR_DURING_TEST},
        {CRYPTO_ERROR_UNKNOWN, CRYPTO_MSG_ERROR_UNKNOWN},
        {CRYPTO_ERROR_NOT_IMPLEMENTED, CRYPTO_MSG_ERROR_NOT_IMPLEMENTED},
        {CRYPTO_ERROR_NO_MEMORY, CRYPTO_MSG_ERROR_NO_MEMORY},
        {CRYPTO_ERROR_INVALID_PARAMETER, CRYPTO_MSG_ERROR_INVALID_PARAMETER},
        {CRYPTO_ERROR_SHARED_KEY, CRYPTO_MSG_ERROR_SHARED_KEY},
        {CRYPTO_ERROR_TOO_MANY_ITERATIONS, CRYPTO_MSG_ERROR_TOO_MANY_ITERATIONS}
    };

    if (Status >= CRYPTO_STATUS_TYPE_SIZE || mapping[Status].string == NULL) {
        return "Unrecognized CRYPTO_STATUS";
    } else {
        return mapping[Status].string;
    }
};

/*
 * @param encode_A Alice's message encryption  
*/
void encode_A(const uint32_t* pk, const unsigned char* seed, unsigned char* m)
{  
    unsigned int i = 0, j;
        
#if defined(GENERIC_IMPLEMENTATION)
    for (j = 0; j < 1024; j += 4) {        
        m[i]   = (unsigned char)(pk[j] & 0xFF);
        m[i+1] = (unsigned char)((pk[j] >> 8) | ((pk[j+1] & 0x03) << 6));
        m[i+2] = (unsigned char)((pk[j+1] >> 2) & 0xFF);
        m[i+3] = (unsigned char)((pk[j+1] >> 10) | ((pk[j+2] & 0x0F) << 4));
        m[i+4] = (unsigned char)((pk[j+2] >> 4) & 0xFF);
        m[i+5] = (unsigned char)((pk[j+2] >> 12) | ((pk[j+3] & 0x3F) << 2));
        m[i+6] = (unsigned char)(pk[j+3] >> 6);
        i += 7;
    }
    
#elif defined(ASM_SUPPORT) && (SIMD_SUPPORT == AVX2_SUPPORT) 
    encode_asm(pk, m);
    i = 1792;
#endif

    for (j = 0; j < 32; j++) {
        m[i+j] = seed[j];
    }
}

/*
 * @param decode_A Alice's message decryption  
*/
void decode_A(const unsigned char* m, uint32_t *pk, unsigned char* seed)
{  
    unsigned int i = 0, j;
    
#if defined(GENERIC_IMPLEMENTATION)
    for (j = 0; j < 1024; j += 4) {        
        pk[j]   = ((uint32_t)m[i] | (((uint32_t)m[i+1] & 0x3F) << 8));
        pk[j+1] = (((uint32_t)m[i+1] >> 6) | ((uint32_t)m[i+2] << 2) | (((uint32_t)m[i+3] & 0x0F) << 10));
        pk[j+2] = (((uint32_t)m[i+3] >> 4) | ((uint32_t)m[i+4] << 4) | (((uint32_t)m[i+5] & 0x03) << 12));
        pk[j+3] = (((uint32_t)m[i+5] >> 2) | ((uint32_t)m[i+6] << 6));
        i += 7;
    }
    
#elif defined(ASM_SUPPORT) && (SIMD_SUPPORT == AVX2_SUPPORT) 
    decode_asm(m, pk);
    i = 1792;
#endif

    for (j = 0; j < 32; j++) {
        seed[j] = m[i+j];
    }
}

/*
 * @param encode_B Bob's message encryption  
*/
void encode_B(const uint32_t* pk, const uint32_t* rvec, unsigned char* m)
{  
    unsigned int i = 0, j;
    
#if defined(GENERIC_IMPLEMENTATION) 
    for (j = 0; j < 1024; j += 4) {        
        m[i]   = (unsigned char)(pk[j] & 0xFF);
        m[i+1] = (unsigned char)((pk[j] >> 8) | ((pk[j+1] & 0x03) << 6));
        m[i+2] = (unsigned char)((pk[j+1] >> 2) & 0xFF);
        m[i+3] = (unsigned char)((pk[j+1] >> 10) | ((pk[j+2] & 0x0F) << 4));
        m[i+4] = (unsigned char)((pk[j+2] >> 4) & 0xFF);
        m[i+5] = (unsigned char)((pk[j+2] >> 12) | ((pk[j+3] & 0x3F) << 2));
        m[i+6] = (unsigned char)(pk[j+3] >> 6);
        i += 7;
    }
    
#elif defined(ASM_SUPPORT) && (SIMD_SUPPORT == AVX2_SUPPORT) 
    encode_asm(pk, m);
#endif

    i = 0;
    for (j = 0; j < 1024/4; j++) {
        m[1792+j] = (unsigned char)(rvec[i] | (rvec[i+1] << 2) | (rvec[i+2] << 4) | (rvec[i+3] << 6));
        i += 4;
    }
}

/*
 * @param decode_B Bob's message decryption  
*/
void decode_B(unsigned char* m, uint32_t* pk, uint32_t* rvec)
{  
    unsigned int i = 0, j;
    
#if defined(GENERIC_IMPLEMENTATION) 
    for (j = 0; j < 1024; j += 4) {        
        pk[j]   = ((uint32_t)m[i] | (((uint32_t)m[i+1] & 0x3F) << 8));
        pk[j+1] = (((uint32_t)m[i+1] >> 6) | ((uint32_t)m[i+2] << 2) | (((uint32_t)m[i+3] & 0x0F) << 10));
        pk[j+2] = (((uint32_t)m[i+3] >> 4) | ((uint32_t)m[i+4] << 4) | (((uint32_t)m[i+5] & 0x03) << 12));
        pk[j+3] = (((uint32_t)m[i+5] >> 2) | ((uint32_t)m[i+6] << 6));
        i += 7;
    }
    
#elif defined(ASM_SUPPORT) && (SIMD_SUPPORT == AVX2_SUPPORT) 
    decode_asm(m, pk);
    i = 1792;
#endif
    
    i = 0;
    for (j = 0; j < 1024/4; j++) {
        rvec[i]   = (uint32_t)(m[1792+j] & 0x03);
        rvec[i+1] = (uint32_t)((m[1792+j] >> 2) & 0x03);
        rvec[i+2] = (uint32_t)((m[1792+j] >> 4) & 0x03);
        rvec[i+3] = (uint32_t)(m[1792+j] >> 6);
        i += 4;
    }
}

/*
 * @param Abs Computes absolute value 
*/
static __inline uint32_t Abs(int32_t value)
{ 
    uint32_t mask;

    mask = (uint32_t)(value >> 31);
    return ((mask ^ value) - mask);
}

/*
 * @param HelpRec Reconciliation helper
 * @note Move to kex.h
*/
CRYPTO_STATUS HelpRec(const uint32_t* x, uint32_t* rvec, const unsigned char* seed, unsigned int nonce, StreamOutput StreamOutputFunction)
{  
    unsigned int i, j, norm;
    unsigned char bit, random_bits[32], nce[8] = {0};
    uint32_t v0[4], v1[4];
    CRYPTO_STATUS Status = CRYPTO_ERROR_UNKNOWN;
    
    nce[1] = (unsigned char)nonce;                
    Status = stream_output(seed, ERROR_SEED_BYTES, nce, NONCE_SEED_BYTES, 32, random_bits, StreamOutputFunction);
    if (Status != CRYPTO_SUCCESS) {
        clear_words((void*)random_bits, NBYTES_TO_NWORDS(32));
        return Status;
    }    

#if defined(ASM_SUPPORT) && (SIMD_SUPPORT == AVX2_SUPPORT)         
    helprec_asm(x, rvec, random_bits);
#else   

    for (i = 0; i < 256; i++) {
        bit = 1 & (random_bits[i >> 3] >> (i & 0x07));
        rvec[i]     = (x[i]     << 1) - bit;  
        rvec[i+256] = (x[i+256] << 1) - bit;
        rvec[i+512] = (x[i+512] << 1) - bit;
        rvec[i+768] = (x[i+768] << 1) - bit; 

        norm = 0;
        v0[0] = 4; v0[1] = 4; v0[2] = 4; v0[3] = 4;
        v1[0] = 3; v1[1] = 3; v1[2] = 3; v1[3] = 3; 
        for (j = 0; j < 4; j++) {
            v0[j] -= (rvec[i+256*j] - PARAMETER_Q4 ) >> 31;
            v0[j] -= (rvec[i+256*j] - PARAMETER_3Q4) >> 31;
            v0[j] -= (rvec[i+256*j] - PARAMETER_5Q4) >> 31;
            v0[j] -= (rvec[i+256*j] - PARAMETER_7Q4) >> 31;
            v1[j] -= (rvec[i+256*j] - PARAMETER_Q2 ) >> 31;
            v1[j] -= (rvec[i+256*j] - PARAMETER_Q  ) >> 31;
            v1[j] -= (rvec[i+256*j] - PARAMETER_3Q2) >> 31;
            norm += Abs(2*rvec[i+256*j] - PARAMETER_Q*v0[j]);
        }
/*
 * @note If norm < q then norm = 0xff...ff, else norm = 0
*/
        norm = (uint32_t)((int32_t)(norm - PARAMETER_Q) >> 31);    
        v0[0] = (norm & (v0[0] ^ v1[0])) ^ v1[0];
        v0[1] = (norm & (v0[1] ^ v1[1])) ^ v1[1];
        v0[2] = (norm & (v0[2] ^ v1[2])) ^ v1[2];
        v0[3] = (norm & (v0[3] ^ v1[3])) ^ v1[3];
        rvec[i]     = (v0[0] - v0[3]) & 0x03;
        rvec[i+256] = (v0[1] - v0[3]) & 0x03;
        rvec[i+512] = (v0[2] - v0[3]) & 0x03;
        rvec[i+768] = ((v0[3] << 1) + (1 & ~norm)) & 0x03;
    }
#endif

    return Status;
}

/*
 * @param LDDecode Performs low-density decoding
*/
static __inline uint32_t LDDecode(int32_t* t)
{  
    unsigned int i, norm = 0;
    uint32_t mask1, mask2, value;
    int32_t cneg = -8*PARAMETER_Q;
    
	for (i = 0; i < 4; i++) { 
        mask1 = t[i] >> 31;                                    // If t[i] < 0 then mask2 = 0xff...ff, else mask2 = 0
        mask2 = (4*PARAMETER_Q - (int32_t)Abs(t[i])) >> 31;    // If 4*PARAMETER_Q > Abs(t[i]) then mask2 = 0, else mask2 = 0xff...ff

        value = ((mask1 & (8*PARAMETER_Q ^ cneg)) ^ cneg);
		norm += Abs(t[i] + (mask2 & value));
    }

    return ((8*PARAMETER_Q - norm) >> 31) ^ 1;                 // If norm < PARAMETER_Q then return 1, else return 0
};

/*
 * @param Rec Reconciles crypto exchange
*/
void Rec(const uint32_t *x, const uint32_t* rvec, unsigned char *key)               
{  
#if defined(GENERIC_IMPLEMENTATION)
    unsigned int i;
    uint32_t t[4];

    for (i = 0; i < 32; i++) {
        key[i] = 0;
    }
    for (i = 0; i < 256; i++) {        
        t[0] = 8*x[i]     - (2*rvec[i] + rvec[i+768]) * PARAMETER_Q;
        t[1] = 8*x[i+256] - (2*rvec[i+256] + rvec[i+768]) * PARAMETER_Q;
        t[2] = 8*x[i+512] - (2*rvec[i+512] + rvec[i+768]) * PARAMETER_Q;
        t[3] = 8*x[i+768] - (rvec[i+768]) * PARAMETER_Q;
      
        key[i >> 3] |= (unsigned char)LDDecode((int32_t*)t) << (i & 0x07);
    }
    
#elif defined(ASM_SUPPORT) && (SIMD_SUPPORT == AVX2_SUPPORT) 
    rec_asm(x, rvec, key);
#endif
}

/*
 * @param get_error Samples for errors
*/
CRYPTO_STATUS get_error(int32_t* e, unsigned char* seed, unsigned int nonce, StreamOutput StreamOutputFunction)              
{  
    unsigned char stream[3*PARAMETER_N];    
    uint32_t* pstream = (uint32_t*)&stream;   
    uint32_t acc1, acc2, temp;  
    uint8_t *pacc1 = (uint8_t*)&acc1, *pacc2 = (uint8_t*)&acc2;
    unsigned char nce[8] = {0};
    unsigned int i, j;
    CRYPTO_STATUS Status = CRYPTO_ERROR_UNKNOWN;
    
    nce[0] = (unsigned char)nonce;
    Status = stream_output(seed, ERROR_SEED_BYTES, nce, NONCE_SEED_BYTES, 3*PARAMETER_N, stream, StreamOutputFunction);
    if (Status != CRYPTO_SUCCESS) {
        clear_words((void*)stream, NBYTES_TO_NWORDS(3*PARAMETER_N));
        return Status;
    }    

#if defined(ASM_SUPPORT) && (SIMD_SUPPORT == AVX2_SUPPORT)         
    error_sampling_asm(stream, e);
#else    
    for (i = 0; i < PARAMETER_N/4; i++)
    {
        acc1 = 0;
        acc2 = 0;
        for (j = 0; j < 8; j++) {
            acc1 += (pstream[i] >> j) & 0x01010101;
            acc2 += (pstream[i+PARAMETER_N/4] >> j) & 0x01010101;
        }
        for (j = 0; j < 4; j++) {
            temp = pstream[i+2*PARAMETER_N/4] >> j;
            acc1 += temp & 0x01010101;
            acc2 += (temp >> 4) & 0x01010101;
        }
        e[2*i]   = pacc1[0] - pacc1[1];                               
        e[2*i+1] = pacc1[2] - pacc1[3];
        e[2*i+PARAMETER_N/2]   = pacc2[0] - pacc2[1];               
        e[2*i+PARAMETER_N/2+1] = pacc2[2] - pacc2[3];
    }
#endif

    return Status;
}

/*
 * @param generate_a Generates temporary variable a
 * @note Rename this variable
*/
CRYPTO_STATUS generate_a(uint32_t* a, const unsigned char* seed, ExtendableOutput ExtendableOutputFunction)             
{  

    return extended_output(seed, SEED_BYTES, PARAMETER_N, a, ExtendableOutputFunction);
}

/*
 * @param KeyGeneration_A Alice's SecretKeyA key generation and PublicKeyA computation
 * @return Produces the private key SecretKeyA as 32-bit signed 1024-element array (4096 bytes in total)
 * @note public key PublicKeyA occupies 1824 bytes
*/
CRYPTO_STATUS KeyGeneration_A(int32_t* SecretKeyA, unsigned char* PublicKeyA, PLatticeCryptoStruct pLatticeCrypto) 
{   
    uint32_t a[PARAMETER_N];
    int32_t e[PARAMETER_N];
    unsigned char seed[SEED_BYTES], error_seed[ERROR_SEED_BYTES];
    CRYPTO_STATUS Status = CRYPTO_ERROR_UNKNOWN;

    Status = random_bytes(SEED_BYTES, seed, pLatticeCrypto->RandomBytesFunction);   
    if (Status != CRYPTO_SUCCESS) {
        return Status;
    }
    Status = random_bytes(ERROR_SEED_BYTES, error_seed, pLatticeCrypto->RandomBytesFunction);   
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }

    Status = generate_a(a, seed, pLatticeCrypto->ExtendableOutputFunction);
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }

    Status = get_error(SecretKeyA, error_seed, 0, pLatticeCrypto->StreamOutputFunction);  
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }
    Status = get_error(e, error_seed, 1, pLatticeCrypto->StreamOutputFunction);   
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }
    NTT_CT_std2rev_12289(SecretKeyA, psi_rev_ntt1024_12289, PARAMETER_N); 
    NTT_CT_std2rev_12289(e, psi_rev_ntt1024_12289, PARAMETER_N);
    smul(e, 3, PARAMETER_N);

    pmuladd((int32_t*)a, SecretKeyA, e, (int32_t*)a, PARAMETER_N); 
    correction((int32_t*)a, PARAMETER_Q, PARAMETER_N);
    encode_A(a, seed, PublicKeyA);
    
cleanup:
    clear_words((void*)e, NBYTES_TO_NWORDS(4*PARAMETER_N));
    clear_words((void*)error_seed, NBYTES_TO_NWORDS(ERROR_SEED_BYTES));

    return Status;
}

/*
 * @param SecretAgreement_B Bob's key generation from Alice's 1824 byte PublicKeyA and shared secret computation
 * @return public key PublicKeyB (2048 bytes) and SharedSecretB (256 bits)
 * @note Rename this variable
*/
CRYPTO_STATUS SecretAgreement_B(unsigned char* PublicKeyA, unsigned char* SharedSecretB, unsigned char* PublicKeyB, PLatticeCryptoStruct pLatticeCrypto) 
{ 
    uint32_t pk_A[PARAMETER_N], a[PARAMETER_N], v[PARAMETER_N], r[PARAMETER_N];
    int32_t sk_B[PARAMETER_N], e[PARAMETER_N];
    unsigned char seed[SEED_BYTES], error_seed[ERROR_SEED_BYTES];
    CRYPTO_STATUS Status = CRYPTO_ERROR_UNKNOWN;

    decode_A(PublicKeyA, pk_A, seed);
    Status = random_bytes(ERROR_SEED_BYTES, error_seed, pLatticeCrypto->RandomBytesFunction); 
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }

    Status = generate_a(a, seed, pLatticeCrypto->ExtendableOutputFunction);
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }

    Status = get_error(sk_B, error_seed, 0, pLatticeCrypto->StreamOutputFunction);  
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }
    Status = get_error(e, error_seed, 1, pLatticeCrypto->StreamOutputFunction);
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }   
    NTT_CT_std2rev_12289(sk_B, psi_rev_ntt1024_12289, PARAMETER_N); 
    NTT_CT_std2rev_12289(e, psi_rev_ntt1024_12289, PARAMETER_N);
    smul(e, 3, PARAMETER_N);

    pmuladd((int32_t*)a, sk_B, e, (int32_t*)a, PARAMETER_N); 
    correction((int32_t*)a, PARAMETER_Q, PARAMETER_N);
     
    Status = get_error(e, error_seed, 2, pLatticeCrypto->StreamOutputFunction);  
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }   
    NTT_CT_std2rev_12289(e, psi_rev_ntt1024_12289, PARAMETER_N); 
    smul(e, 81, PARAMETER_N);
    
    pmuladd((int32_t*)pk_A, sk_B, e, (int32_t*)v, PARAMETER_N);    
    INTT_GS_rev2std_12289((int32_t*)v, omegainv_rev_ntt1024_12289, omegainv10N_rev_ntt1024_12289, Ninv11_ntt1024_12289, PARAMETER_N);
    two_reduce12289((int32_t*)v, PARAMETER_N);
#if defined(GENERIC_IMPLEMENTATION)
    correction((int32_t*)v, PARAMETER_Q, PARAMETER_N); 
#endif

    Status = HelpRec(v, r, error_seed, 3, pLatticeCrypto->StreamOutputFunction); 
    if (Status != CRYPTO_SUCCESS) {
        goto cleanup;
    }   
    Rec(v, r, SharedSecretB);
    encode_B(a, r, PublicKeyB);
    
cleanup:
    clear_words((void*)sk_B, NBYTES_TO_NWORDS(4*PARAMETER_N));
    clear_words((void*)e, NBYTES_TO_NWORDS(4*PARAMETER_N));
    clear_words((void*)error_seed, NBYTES_TO_NWORDS(ERROR_SEED_BYTES));
    clear_words((void*)a, NBYTES_TO_NWORDS(4*PARAMETER_N));
    clear_words((void*)v, NBYTES_TO_NWORDS(4*PARAMETER_N));
    clear_words((void*)r, NBYTES_TO_NWORDS(4*PARAMETER_N));

    return Status;
}

/*
 * @param SecretAgreement_A Computes shared secret SharedSecretA using Bob's 2048-byte public key PublicKeyB and Alice's 256-bit private key SecretKeyA.
 * @return Outputs 256-bit SharedSecretA
*/
CRYPTO_STATUS SecretAgreement_A(unsigned char* PublicKeyB, int32_t* SecretKeyA, unsigned char* SharedSecretA) 
{ 
    uint32_t u[PARAMETER_N], r[PARAMETER_N];
    CRYPTO_STATUS Status = CRYPTO_SUCCESS;

    decode_B(PublicKeyB, u, r);
    
    pmul(SecretKeyA, (int32_t*)u, (int32_t*)u, PARAMETER_N);       
    INTT_GS_rev2std_12289((int32_t*)u, omegainv_rev_ntt1024_12289, omegainv10N_rev_ntt1024_12289, Ninv11_ntt1024_12289, PARAMETER_N);
    two_reduce12289((int32_t*)u, PARAMETER_N);
#if defined(GENERIC_IMPLEMENTATION)
    correction((int32_t*)u, PARAMETER_Q, PARAMETER_N); 
#endif

    Rec(u, r, SharedSecretA);
    
/*
 * @param clear_words Cleans up the registers
*/
    clear_words((void*)u, NBYTES_TO_NWORDS(4*PARAMETER_N));
    clear_words((void*)r, NBYTES_TO_NWORDS(4*PARAMETER_N));

    return Status;
}
