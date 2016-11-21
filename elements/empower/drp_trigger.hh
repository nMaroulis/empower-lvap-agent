#ifndef CLICK_EMPOWER_DRP_HH
#define CLICK_EMPOWER_DRP_HH
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

    DrpTrigger(EtherAddress, uint32_t, uint16_t ,bool ,  EmpowerLVAPManager *,EmpowerRXStats *);
    ~DrpTrigger();
    String unparse();
    inline bool operator==(const DrpTrigger &b) {
        return  (_eth == b._eth) && (_val == b._val);
    }

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_DRP_HH */
