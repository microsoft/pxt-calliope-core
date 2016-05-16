#include "pxt.h"
#include <map>

MicroBit uBit;

namespace pxt {
    int incr(uint32_t e)
    {
      if (e) {
        if (hasVTable(e))
          ((RefObject*)e)->ref();
        else
          ((RefCounted*)e)->incr();
      }
      return e;
    }

    void decr(uint32_t e)
    {
      if (e) {
        if (hasVTable(e))
          ((RefObject*)e)->unref();
        else
          ((RefCounted*)e)->decr();
      }
    }

    Action mkAction(int reflen, int totallen, int startptr)
    {
      check(0 <= reflen && reflen <= totallen, ERR_SIZE, 1);
      check(reflen <= totallen && totallen <= 255, ERR_SIZE, 2);
      check(bytecode[startptr] == 0xffff, ERR_INVALID_BINARY_HEADER, 3);
      check(bytecode[startptr + 1] == 0, ERR_INVALID_BINARY_HEADER, 4);

      uint32_t tmp = (uint32_t)&bytecode[startptr];

      if (totallen == 0) {
        return tmp; // no closure needed
      }

      void *ptr = ::operator new(sizeof(RefAction) + totallen * sizeof(uint32_t));
      RefAction *r = new (ptr) RefAction();
      r->len = totallen;
      r->reflen = reflen;
      r->func = (ActionCB)((tmp + 4) | 1);
      memset(r->fields, 0, r->len * sizeof(uint32_t));

      return (Action)r;
    }

    void runAction1(Action a, int arg)
    {
      if (hasVTable(a))
        ((RefAction*)a)->runCore(arg);
      else {
        check(*(uint16_t*)a == 0xffff, ERR_INVALID_BINARY_HEADER, 4);
        ((ActionCB)((a + 4) | 1))(NULL, NULL, arg);
      }
    }

    void runAction0(Action a)
    {
      runAction1(a, 0);
    }

    RefRecord* mkRecord(int reflen, int totallen)
    {
      check(0 <= reflen && reflen <= totallen, ERR_SIZE, 1);
      check(reflen <= totallen && totallen <= 255, ERR_SIZE, 2);

      void *ptr = ::operator new(sizeof(RefRecord) + totallen * sizeof(uint32_t));
      RefRecord *r = new (ptr) RefRecord();
      r->len = totallen;
      r->reflen = reflen;
      memset(r->fields, 0, r->len * sizeof(uint32_t));
      return r;
    }

    uint32_t RefRecord::ld(int idx)
    {
      check(reflen <= idx && idx < len, ERR_OUT_OF_BOUNDS, 1);
      return fields[idx];
    }

    uint32_t RefRecord::ldref(int idx)
    {
      //printf("LD %p len=%d reflen=%d idx=%d\n", this, len, reflen, idx);
      check(0 <= idx && idx < reflen, ERR_OUT_OF_BOUNDS, 2);
      uint32_t tmp = fields[idx];
      incr(tmp);
      return tmp;
    }

    void RefRecord::st(int idx, uint32_t v)
    {
      check(reflen <= idx && idx < len, ERR_OUT_OF_BOUNDS, 3);
      fields[idx] = v;
    }

    void RefRecord::stref(int idx, uint32_t v)
    {
      //printf("ST %p len=%d reflen=%d idx=%d\n", this, len, reflen, idx);
      check(0 <= idx && idx < reflen, ERR_OUT_OF_BOUNDS, 4);
      decr(fields[idx]);
      fields[idx] = v;
    }

    RefRecord::~RefRecord()
    {
      //printf("DELREC: %p\n", this);
      for (int i = 0; i < this->reflen; ++i) {
        decr(fields[i]);
        fields[i] = 0;
      }
    }

    void RefRecord::print()
    {
      printf("RefRecord %p r=%d size=%d (%d refs)\n", this, refcnt, len, reflen);
    }


    void RefCollection::push(uint32_t x) {
      if (flags & 1) incr(x);
      data.push_back(x);
    }

    uint32_t RefCollection::getAt(int x) {
      if (in_range(x)) {
        uint32_t tmp = data.at(x);
        if (flags & 1) incr(tmp);
        return tmp;
      }
      else {
        error(ERR_OUT_OF_BOUNDS);
        return 0;
      }
    }

    void RefCollection::removeAt(int x) {
      if (!in_range(x))
        return;

      if (flags & 1) decr(data.at(x));
      data.erase(data.begin()+x);
    }

    void RefCollection::setAt(int x, uint32_t y) {
      if (!in_range(x))
        return;

      if (flags & 1) {
        decr(data.at(x));
        incr(y);
      }
      data.at(x) = y;
    }

    int RefCollection::indexOf(uint32_t x, int start) {
      if (!in_range(start))
        return -1;

      if (flags & 2) {
        StringData *xx = (StringData*)x;
        for (uint32_t i = start; i < data.size(); ++i) {
          StringData *ee = (StringData*)data.at(i);
          if (xx->len == ee->len && memcmp(xx->data, ee->data, xx->len) == 0)
            return (int)i;
        }
      } else {
        for (uint32_t i = start; i < data.size(); ++i)
          if (data.at(i) == x)
            return (int)i;
      }

      return -1;
    }

    int RefCollection::removeElement(uint32_t x) {
      int idx = indexOf(x, 0);
      if (idx >= 0) {
        removeAt(idx);
        return 1;
      }
      return 0;
    }

    void RefObject::print()
    {
      printf("RefObject %p\n", this);
    }

    RefCollection::~RefCollection()
    {
      // printf("KILL "); this->print();
      if (flags & 1)
        for (uint32_t i = 0; i < data.size(); ++i) {
          decr(data[i]);
          data[i] = 0;
        }
      data.resize(0);
    }

    void RefCollection::print()
    {
      printf("RefCollection %p r=%d flags=%d size=%d [%p, ...]\n", this, refcnt, flags, data.size(), data.size() > 0 ? data[0] : 0);
    }

    // fields[] contain captured locals
    RefAction::~RefAction()
    {
      for (int i = 0; i < this->reflen; ++i) {
        decr(fields[i]);
        fields[i] = 0;
      }
    }

    void RefAction::print()
    {
      printf("RefAction %p r=%d pc=0x%lx size=%d (%d refs)\n", this, refcnt, (const uint8_t*)func - (const uint8_t*)bytecode, len, reflen);
    }

    void RefLocal::print()
    {
      printf("RefLocal %p r=%d v=%d\n", this, refcnt, v);
    }

    void RefRefLocal::print()
    {
      printf("RefRefLocal %p r=%d v=%p\n", this, refcnt, (void*)v);
    }

    RefRefLocal::~RefRefLocal()
    {
      decr(v);
    }

#ifdef DEBUG_MEMLEAKS
  std::set<RefObject*> allptrs;
  void debugMemLeaks()
  {
    printf("LIVE POINTERS:\n");
    for(std::set<RefObject*>::iterator itr = allptrs.begin();itr!=allptrs.end();itr++)
    {
      (*itr)->print();
    }    
    printf("\n");
  }
#else
  void debugMemLeaks() {}
#endif


    // ---------------------------------------------------------------------------
    // An adapter for the API expected by the run-time.
    // ---------------------------------------------------------------------------

    map<pair<int, int>, Action> handlersMap;
    
    MicroBitEvent lastEvent;

    // We have the invariant that if [dispatchEvent] is registered against the DAL
    // for a given event, then [handlersMap] contains a valid entry for that
    // event.
    void dispatchEvent(MicroBitEvent e) {
      
      lastEvent = e;
      
      Action curr = handlersMap[{ e.source, e.value }];
      if (curr)
        runAction1(curr, e.value);

      curr = handlersMap[{ e.source, MICROBIT_EVT_ANY }];
      if (curr)
        runAction1(curr, e.value);
    }

    void registerWithDal(int id, int event, Action a) {
      Action prev = handlersMap[{ id, event }];
      if (prev)
        decr(prev);
      else
        uBit.messageBus.listen(id, event, dispatchEvent);
      incr(a);
      handlersMap[{ id, event }] = a;
    }

    void fiberDone(void *a)
    {
      decr((Action)a);
      release_fiber();
    }


    void runInBackground(Action a) {
      if (a != 0) {
        incr(a);
        create_fiber((void(*)(void*))runAction0, (void*)a, fiberDone);
      }
    }
  

  void error(ERROR code, int subcode)
  {
    printf("Error: %d [%d]\n", code, subcode);
    uBit.panic(42);
  }

  uint16_t *bytecode;
  uint32_t *globals;
  int numGlobals;

  uint32_t *allocate(uint16_t sz)
  {
    uint32_t *arr = new uint32_t[sz];
    memset(arr, 0, sz * 4);
    return arr;
  }

  void checkStr(bool cond, const char *msg)
  {
    if (!cond) {
      while (true) {
        uBit.display.scroll(msg, 100);
        uBit.sleep(100);
      }
    }
  }

  int templateHash()
  {
    return ((int*)bytecode)[4];
  }

  int programHash()
  {
    return ((int*)bytecode)[6];
  }

  void exec_binary(uint16_t *pc)
  {
    // XXX re-enable once the calibration code is fixed and [editor/embedded.ts]
    // properly prepends a call to [internal_main].
    // ::touch_develop::internal_main();

    // unique group for radio based on source hash
    // ::touch_develop::micro_bit::radioDefaultGroup = programHash();
    
    // repeat error 4 times and restart as needed
    microbit_panic_timeout(4);
    
    uint32_t ver = *pc++;
    checkStr(ver == 0x4208, ":( Bad runtime version");
    numGlobals = *pc++;
    globals = allocate(numGlobals);

    bytecode = *((uint16_t**)pc);  // the actual bytecode is here
    pc += 2;

    // just compare the first word
    checkStr(((uint32_t*)bytecode)[0] == 0x923B8E70 &&
             templateHash() == ((int*)pc)[0],
             ":( Failed partial flash");

    uint32_t startptr = (uint32_t)bytecode;
    startptr += 48; // header
    startptr |= 1; // Thumb state

    ((uint32_t (*)())startptr)();

#ifdef DEBUG_MEMLEAKS
    bitvm::debugMemLeaks();
#endif

    return;
  }

  void start()
  {
    exec_binary((uint16_t*)functionsAndBytecode);
  }
}  

// vim: ts=2 sw=2 expandtab
