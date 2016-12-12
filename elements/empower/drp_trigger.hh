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
    
    bool _dispatched;
    int _val;
    EtherAddress _eth1;
    EtherAddress _eth2;
    String _rule;

    DrpTrigger(EtherAddress, EtherAddress, uint32_t, uint16_t, String,bool, EmpowerLVAPManager *, EmpowerRXStats *);
    ~DrpTrigger();
    String unparse();
    inline bool operator==(const DrpTrigger &b) {
        return  (_eth1 == b._eth1) && (_eth2  == b._eth2) && (_rule  == b._rule);
    }

};

CLICK_ENDDECLS
#endif /* CLICK_EMPOWER_DRP_HH */
