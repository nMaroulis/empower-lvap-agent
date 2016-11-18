#ifndef CLICK_EMPOWERDUMP_HH
#define CLICK_EMPOWERDUMP_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "empowerpacket.hh"
#include "dstinfo.hh"
#include "trigger.hh"
CLICK_DECLS

class DrpTrigger: public Trigger {
public:

    int _val;
    bool _dispatched;

    DrpTrigger(EmpowerLVAPManager *,EmpowerRXStats *);
    ~DrpTrigger();

    bool matches(const DstInfo* nfo);

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_DUMP_HH */
