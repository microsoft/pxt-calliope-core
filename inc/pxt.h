#ifndef __PXT_H
#define __PXT_H

// #define DEBUG_MEMLEAKS 1

#pragma GCC diagnostic ignored "-Wunused-parameter"

#include "MicroBit.h"
#include "MicroBitImage.h"
#include "ManagedString.h"
#include "ManagedType.h"

#define printf(...) uBit.serial.printf(__VA_ARGS__)
// #define printf(...)

#include <stdio.h>
#include <string.h>
#include <vector>
#include <stdint.h>

#ifdef DEBUG_MEMLEAKS
#include <set>
#endif

extern MicroBit uBit;

namespace pxt {
  typedef uint32_t Action;
  typedef uint32_t ImageLiteral;


  typedef enum {
    ERR_INVALID_BINARY_HEADER = 5,
    ERR_OUT_OF_BOUNDS = 8,
    ERR_REF_DELETED = 7,
    ERR_SIZE = 9,
  } ERROR;

  extern const uint32_t functionsAndBytecode[];
  extern uint32_t *globals;
  extern int numGlobals;
  extern uint16_t *bytecode;
  class RefRecord;

  // Utility functions
  extern MicroBitEvent lastEvent;
  void registerWithDal(int id, int event, Action a);
  void runInBackground(Action a);
  void runAction0(Action a);
  void runAction1(Action a, int arg);
  Action mkAction(int reflen, int totallen, int startptr);
  void error(ERROR code, int subcode = 0);
  void exec_binary(uint16_t *pc);
  void start();
  void debugMemLeaks();
  // allocate [sz] words and clear them
  uint32_t *allocate(uint16_t sz);
  int templateHash();
  int programHash();
  RefRecord* mkRecord(int reflen, int totallen);

  // The standard calling convention is:
  //   - when a pointer is loaded from a local/global/field etc, and incr()ed
  //     (in other words, its presence on stack counts as a reference)
  //   - after a function call, all pointers are popped off the stack and decr()ed
  // This does not apply to the RefRecord and st/ld(ref) methods - they unref()
  // the RefRecord* this.
  int incr(uint32_t e);
  void decr(uint32_t e);

  inline void *ptrOfLiteral(int offset)
  {
    return &bytecode[offset];
  }

  inline ImageData* imageBytes(int offset)
  {
    return (ImageData*)(void*)&bytecode[offset];
  }

  // Checks if object has a VTable, or if its RefCounted* from the runtime.
  inline bool hasVTable(uint32_t e)
  {
    return (*((uint32_t*)e) & 1) == 0;
  }

  inline void check(int cond, ERROR code, int subcode = 0)
  {
    if (!cond) error(code, subcode);
  }


#ifdef DEBUG_MEMLEAKS
  class RefObject;
  extern std::set<RefObject*> allptrs;
#endif

  // A base abstract class for ref-counted objects.
  class RefObject
  {
  public:
    uint16_t refcnt;

    RefObject()
    {
      refcnt = 1;
#ifdef DEBUG_MEMLEAKS
      allptrs.insert(this);
#endif
    }

    // Call to disable pointer tracking on the current instance.
    void canLeak()
    {
#ifdef DEBUG_MEMLEAKS
      allptrs.erase(this);
#endif
    }

    // Increment/decrement the ref-count. Decrementing to zero deletes the current object.
    inline void ref()
    {
      check(refcnt > 0, ERR_REF_DELETED);
      //printf("INCR "); this->print();
      refcnt++;
    }

    inline void unref()
    {
      //printf("DECR "); this->print();
      if (--refcnt == 0) {
        delete this;
      }
    }

    virtual void print();

    virtual ~RefObject()
    {
      // This is just a base class for ref-counted objects.
      // There is nothing to free yet, but derived classes will have things to free.
#ifdef DEBUG_MEMLEAKS
      allptrs.erase(this);
#endif
    }

    // This is used by indexOf function, overridden in RefString
    virtual bool equals(RefObject *other)
    {
      return this == other;
    }
  };

  // Ref-counted wrapper around any C++ object.
  template <class T>
  class RefStruct
    : public RefObject
  {
  public:
    T v;

    virtual ~RefStruct() { }

    virtual void print()
    {
      printf("RefStruct %p r=%d\n", this, refcnt);
    }

    RefStruct(const T& i) : v(i) {}
  };

  // A ref-counted collection of either primitive or ref-counted objects (String, Image,
  // user-defined record, another collection)
  class RefCollection
    : public RefObject
  {
  public:
    // 1 - collection of refs (need decr)
    // 2 - collection of strings (in fact we always have 3, never 2 alone)
    uint16_t flags;
    std::vector<uint32_t> data;

    RefCollection(uint16_t f) : flags(f) {}

    virtual ~RefCollection();
    virtual void print();

    inline bool in_range(int x) {
      return (0 <= x && x < (int)data.size());
    }

    inline int length() { return data.size(); }

    void push(uint32_t x);
    uint32_t getAt(int x);
    void removeAt(int x);
    void setAt(int x, uint32_t y);
    int indexOf(uint32_t x, int start);
    int removeElement(uint32_t x);
  };

  // A ref-counted, user-defined Touch Develop object.
  class RefRecord
    : public RefObject
  {
  public:
    // Total number of fields.
    uint8_t len; 
    // Number of fields which are ref-counted pointers; these always come first
    // on the fields[] array.
    uint8_t reflen; 
    // The object is allocated, so that there is space at the end for the fields.
    uint32_t fields[];

    virtual ~RefRecord();
    virtual void print();

    uint32_t ld(int idx);
    uint32_t ldref(int idx);
    void st(int idx, uint32_t v);
    void stref(int idx, uint32_t v);

  };

  class RefAction;
  typedef uint32_t (*ActionCB)(RefAction *, uint32_t *, uint32_t arg);

  // Ref-counted function pointer. It's currently always a ()=>void procedure pointer.
  class RefAction
    : public RefObject
  {
  public:
    // This is the same as for RefRecord.
    uint8_t len;
    uint8_t reflen;
    ActionCB func; // The function pointer
    // fields[] contain captured locals
    uint32_t fields[];

    virtual ~RefAction();
    virtual void print();

    inline void stCore(int idx, uint32_t v)
    {
      //printf("ST [%d] = %d ", idx, v); this->print();
      check(0 <= idx && idx < len, ERR_OUT_OF_BOUNDS, 10);
      check(fields[idx] == 0, ERR_OUT_OF_BOUNDS, 11); // only one assignment permitted
      fields[idx] = v;
    }

    inline uint32_t runCore(int arg) // use runAction*()
    {
      this->ref();
      uint32_t r = this->func(this, &this->fields[0], arg);
      this->unref();
      return r;
    }
  };

  // These two are used to represent locals written from inside inline functions
  class RefLocal
    : public RefObject
  {
  public:
    uint32_t v;

    virtual void print();
    RefLocal() : v(0) {}
  };

  class RefRefLocal
    : public RefObject
  {
  public:
    uint32_t v;

    virtual void print();
    virtual ~RefRefLocal();

    RefRefLocal() : v(0) {}
  };


  // A ref-counted byte buffer
  class RefBuffer
    : public RefObject
  {
  public:
    std::vector<uint8_t> data;

    virtual ~RefBuffer()
    {
      data.resize(0);
    }

    virtual void print()
    {
      printf("RefBuffer %p r=%d size=%d [%p, ...]\n", this, refcnt, data.size(), data.size() > 0 ? data[0] : 0);
    }

    char *cptr() { return (char*)&data[0]; }
    int size() { return data.size(); }

  };

}

// The ARM Thumb generator in the JavaScript code is parsing
// the hex file and looks for the magic numbers as present here.
//
// Then it fetches function pointer addresses from there.
  
#define PXT_SHIMS_BEGIN \
namespace pxt { \
  const uint32_t functionsAndBytecode[] __attribute__((aligned(0x20))) = { \
    0x08010801, 0x42424242, 0x08010801, 0x8de9d83e,

#define PXT_SHIMS_END }; }

#endif

// vim: ts=2 sw=2 expandtab
