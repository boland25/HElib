/* Copyright (C) 2012-2017 IBM Corp.
 * This program is Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. See accompanying LICENSE file.
 */
#ifndef _FHEcontext_H_
#define _FHEcontext_H_
/**
 * @file FHEContext.h
 * @brief Keeps the parameters of an instance of the cryptosystem
 **/
#include "PAlgebra.h"
#include "CModulus.h"
#include "IndexSet.h"
#include "recryption.h"
#include "primeChain.h"

#include <NTL/Lazy.h>

/**
 * @brief Returns smallest parameter m satisfying various constraints:
 * @param k security parameter
 * @param L number of levels 
 * @param c number of columns in key switching matrices
 * @param p characteristic of plaintext space
 * @param d embedding degree (d ==0 or d==1 => no constraint)
 * @param s at least that many plaintext slots
 * @param chosen_m preselected value of m (0 => not preselected)
 * Fails with an error message if no suitable m is found
 * prints an informative message if verbose == true
 **/
long FindM(long k, long nBits, long c, long p, long d, long s, long chosen_m, bool verbose=false);


class EncryptedArray;
/**
 * @class FHEcontext
 * @brief Maintaining the parameters
 **/
class FHEcontext {
  std::vector<Cmodulus> moduli;    // Cmodulus objects for the different primes
  // This is private since the implementation assumes that the list of
  // primes only grows and no prime is ever modified or removed.

public:
  // FHEContext is meant for convenience, not encapsulation: Most data
  // members are public and can be initialized by the application program.

  //! @brief The structure of Zm*
  PAlgebra zMStar;

  //! @brief The structure of Z[X]/(Phi_m(X),p^r)
  PAlgebraMod alMod;

  //! @breif A default EncryptedArray
  const EncryptedArray* ea;
  // FIXME: should this be a unique_ptr??

  //! @brief sqrt(variance) of the LWE error (default=3.2)
  NTL::xdouble stdev;

  /**
   * The "ciphertext primes" are the "normal" primes that are used to
   * represent the public encryption key and ciphertexts. These are all
   * "large" single=precision primes, or bit-size roughly NTL_SP_SIZE bits.
   **/
  IndexSet ctxtPrimes;

  //! A disjoint set of primes, used for key switching. See section 3.1.6
  //! in the design document (key-switching). These too are "large"
  //! single=precision primes, or bit-size close to NTL_SP_SIZE bits.
  IndexSet specialPrimes;

  //! Yet a third set of primes, aimed at allowing modulus-switching with
  //! higher resolution. These are somewhat smaller single-precision
  //! primes, of size from NTL_SP_SIZE-20 to NTL_SP_SIZE-1.
  IndexSet smallPrimes;

  //! A helper table to map required modulo-sizes to primeSets
  ModuliSizes modSizes;
  void setModSizeTable()
       { modSizes.init(moduli, ctxtPrimes, smallPrimes); }

  /**
   * @brief The set of primes for the digits.
   *
   * The different columns in any key-switching matrix contain encryptions
   * of multiplies of the secret key, sk, B1*sk, B2*B1*sk, B3*B2*B1*sk,...
   * with each Bi a product of a few "non-special" primes in the chain. The
   * digits data member indicate which primes correspond to each of the Bi's.
   * These are all IndexSet objects, whose union is the subset ctxtPrimes.
   *
   * The number of Bi's is one less than the number of columns in the key
   * switching matrices (since the 1st column encrypts sk, without any Bi's),
   * but we keep in the digits std::vector also an entry for the primes that do
   * not participate in any Bi (so digits.size() is the same as the number
   * of columns in the key switching matrices).
   * See section 3.1.6 in the design document (key-switching).
  **/
  std::vector<IndexSet> digits; // digits of ctxt/columns of key-switching matrix

  //! Bootstrapping-related data in the context
  RecryptData rcData;
  ThinRecryptData trcData;

  /******************************************************************/
  ~FHEcontext(); // destructor
  FHEcontext(unsigned long m, unsigned long p, unsigned long r,
             const std::vector<long>& gens = std::vector<long>(), 
             const std::vector<long>& ords = std::vector<long>() );  // constructor

  void makeBootstrappable(const NTL::Vec<long>& mvec, long skWht=0,
			  bool conservative=false, bool build_cache=false)
  { 
    rcData.init(*this, mvec, skWht, conservative, build_cache); 
    trcData.init(*this, mvec, skWht, conservative, build_cache); 
  }

  bool isBootstrappable() const 
    { return rcData.alMod != NULL; }

  IndexSet fullPrimes() const 
  {
    return ctxtPrimes | specialPrimes;
  }

  IndexSet allPrimes() const
  {
    return smallPrimes | ctxtPrimes | specialPrimes;
  }

  // retuens first nprimes ctxtPrimes
  IndexSet getCtxtPrimes(long nprimes) const
  {
    long first = ctxtPrimes.first();
    long last = std::min(ctxtPrimes.last(), first + nprimes - 1);
    return IndexSet(first, last);
  }

  // FIXME: replacement for bitsPerLevel...placeholder for now
  long BPL() const { return 30; }

  bool operator==(const FHEcontext& other) const;
  bool operator!=(const FHEcontext& other) const { return !(*this==other); }

  //! @brief The ith small prime in the modulus chain
  long ithPrime(unsigned long i) const 
  { return (i<moduli.size())? moduli[i].getQ() :0; }

  //! @brief Cmodulus object corresponding to ith small prime in the chain
  const Cmodulus& ithModulus(unsigned long i) const { return moduli[i]; }

  //! @brief Total number of small prime in the chain
  long numPrimes() const { return moduli.size(); }

  //! @brief Is num divisible by any of the primes in the chain?
  bool isZeroDivisor(const NTL::ZZ& num) const {
    for (long i: range(moduli.size())) 
      if (divide(num,moduli[i].getQ())) return true;
    return false;
  }

  //! @brief Is p already in the chain?
  bool inChain(long p) const {
    for (long i: range(moduli.size())) 
      if (p==moduli[i].getQ()) return true;
    return false;
  }

  ///@{
  //! @brief The product of all the primes in the given set
  void productOfPrimes(NTL::ZZ& p, const IndexSet& s) const;
  NTL::ZZ productOfPrimes(const IndexSet& s) const {
    NTL::ZZ p;
    productOfPrimes(p,s);
    return p;
  }
  ///@}

  // FIXME: run-time error when ithPrime(i) returns 0
  //! @brief Returns the natural logarithm of the ith prime
  double logOfPrime(unsigned long i) const { return log(ithPrime(i)); }

  //! @brief Returns the natural logarithm of productOfPrimes(s)
  double logOfProduct(const IndexSet& s) const {
    if (s.last() >= numPrimes())
      NTL::Error("FHEContext::logOfProduct: IndexSet has too many rows");

    double ans = 0.0;
    for (long i: s) 
      ans += logOfPrime(i);
    return ans;
  }

  //! @brief An estimate for the security-level
  double securityLevel() const {
    long phim = zMStar.getPhiM();
    double bitsize = logOfProduct(ctxtPrimes | specialPrimes)/log(2.0);
    return (7.2*phim/bitsize -110);
  }

  //! @brief Just add the given prime to the chain
  void AddSmallPrime(long q);
  void AddCtxtPrime(long q);
  void AddSpecialPrime(long q);

  
  ///@{
  /**
     @name I/O routines

  To write out all the data associated with a context, do the following:

  \code
    writeContextBase(str, context);
    str << context;
  \endcode

  The first function call writes out just [m p r gens ords], which is the
  data needed to invoke the context constructor.

  The second call writes out all other information, including the
  stdev field, the prime sequence (including which primes are "special"),
  and the digits info.

  To read in all the data associated with a context, do the following:

  \code
    unsigned long m, p, r;
    std::vector<long> gens, ords;
    
    readContextBase(str, m, p, r, gens, ords);

    FHEcontext context(m, p, r, gens, ords);

    str >> context;
  \endcode

  The call to readContextBase just reads the values m, p, r and the set
  of generators in Zm* /(p) and their order. Then, after constructing the
  context, the >> operator reads in and attaches all other information.
  **/

  //! @brief write [m p r] data
  friend void writeContextBase(std::ostream& str, const FHEcontext& context);

  //! @brief Write all other data
  friend std::ostream& operator<< (std::ostream &str, const FHEcontext& context);

  //! @brief read [m p r] data, needed to construct context
  friend void readContextBase(std::istream& str, unsigned long& m, unsigned long& p, unsigned long& r,
			      std::vector<long>& gens, std::vector<long>& ords);

  //! @brief read all other data associated with context
  friend std::istream& operator>> (std::istream &str, FHEcontext& context);
  ///@}

  friend void writeContextBinary(std::ostream& str, const FHEcontext& context);
  friend void readContextBinary(std::istream& str, FHEcontext& context);

};

//! @brief write [m p r gens ords] data
void writeContextBase(std::ostream& s, const FHEcontext& context);
//! @brief read [m p r gens ords] data, needed to construct context
void readContextBase(std::istream& s, unsigned long& m,
                     unsigned long& p, unsigned long& r,
		     std::vector<long>& gens, std::vector<long>& ords);
std::unique_ptr<FHEcontext> buildContextFromAscii(std::istream& str);

//! @brief write [m p r gens ords] data
void writeContextBaseBinary(std::ostream& str, const FHEcontext& context);
void writeContextBinary(std::ostream& str, const FHEcontext& context);

//! @brief read [m p r gens ords] data, needed to construct context
void readContextBaseBinary(std::istream& s, unsigned long& m,
                           unsigned long& p, unsigned long& r,
                           std::vector<long>& gens, std::vector<long>& ords);
std::unique_ptr<FHEcontext> buildContextFromBinary(std::istream& str);
void readContextBinary(std::istream& str, FHEcontext& context);


// Build modulus chain with nBits worth of ctxt primes, 
// using nDgts digits in key-switching.
// willBeBootstrappable flag is a hack, used to get around some
// circularity when making the context boostrappable.
// resolution ... FIXME

void buildModChain(FHEcontext& context, long nBits, long nDgts=3,
                      bool willBeBootstrappable=false, long resolution=3);

///@}
extern FHEcontext* activeContext; // Should point to the "current" context

#endif // ifndef _FHEcontext_H_
