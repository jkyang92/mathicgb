#ifndef MATHICGB_F4_REDUCER_GUARD
#define MATHICGB_F4_REDUCER_GUARD

#include "Reducer.hpp"
#include "PolyRing.hpp"

class F4Reducer : public Reducer {
public:
  F4Reducer(const PolyRing& ring, std::auto_ptr<Reducer> fallback);

  virtual std::auto_ptr<Poly> classicReduce
  (const Poly& poly, const PolyBasis& basis);

  virtual std::auto_ptr<Poly> classicTailReduce
  (const Poly& poly, const PolyBasis& basis);

  virtual std::auto_ptr<Poly> classicReduceSPoly
  (const Poly& a, const Poly& b, const PolyBasis& basis);

  virtual Poly* regularReduce(
    const_monomial sig,
    const_monomial multiple,
    size_t basisElement,
    const GroebnerBasis& basis);

  virtual std::string description() const;
  virtual size_t getMemoryUse() const;

private:
  std::auto_ptr<Reducer> mFallback;
  const PolyRing& mRing;
};

#endif

// Local Variables:
// compile-command: "make -C .. "
// indent-tabs-mode: nil
// End: