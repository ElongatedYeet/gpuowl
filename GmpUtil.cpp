// Copyright (C) Mihai Preda.

#include "GmpUtil.h"

#include <gmp.h>
#include <cmath>
#include <cassert>

using namespace std;

namespace {

mpz_class mpz(const vector<u32>& words) {
  mpz_class b{};
  mpz_import(b.get_mpz_t(), words.size(), -1 /*order: LSWord first*/, sizeof(u32), 0 /*endianess: native*/, 0 /*nails*/, words.data());
  return b;
}

mpz_class primorial(u32 p) {
  mpz_class b{};
  mpz_primorial_ui(b.get_mpz_t(), p);
  return b;
}

mpz_class powerSmooth(u32 exp, u32 B1) {  
  mpz_class a{exp};
  a *= 256;  // boost 2s.
  for (int k = log2(B1); k >= 1; --k) { a *= primorial(pow(B1, 1.0 / k)); }
  return a;
}

u32 sizeBits(mpz_class a) { return mpz_sizeinbase(a.get_mpz_t(), 2); }

}

vector<bool> bitsMSB(mpz_class a) {
  vector<bool> bits;
  int nBits = sizeBits(a);
  bits.reserve(nBits);
  for (int i = nBits - 1; i >= 0; --i) { bits.push_back(mpz_tstbit(a.get_mpz_t(), i)); }
  assert(int(bits.size()) == nBits);
  return bits;
}

// return GCD(bits - sub, 2^exp - 1) as a decimal string if GCD!=1, or empty string otherwise.
std::string GCD(u32 exp, const std::vector<u32>& words, u32 sub) {
  string ret = mpz_class{gcd((mpz_class{1} << exp) - 1, mpz(words) - sub)}.get_str();
  return ret == "1" ? ""s : ret;
}


// "Rev" means: most significant bit first (at index 0).
vector<bool> powerSmoothMSB(u32 exp, u32 B1) { return bitsMSB(powerSmooth(exp, B1)); }
