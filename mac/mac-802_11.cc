/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*-
 *
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPATSS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Header: /cvsroot/nsnam/ns-2/mac/mac-802_11.cc,v 1.51 2006/01/30 21:27:51 mweigle Exp $
 *
 * Ported from CMU/Monarch's code, nov'98 -Padma.
 * Contributions by:
 *   - Mike Holland
 *   - Sushmita
 */

#include "delay.h"
#include "connector.h"
#include "packet.h"
#include "random.h"
#include "mobilenode.h"

// #define DEBUG 99



// duy, comment it to remove debug
#define DEBUG
#define DEBUG2

#include "arp.h"
#include "ll.h"
#include "mac.h"
#include "mac-timers.h"
#include "mac-802_11.h"
#include "cmu-trace.h"

// Added by Sushmita to support event tracing
#include "agent.h"
#include "basetrace.h"


//tem fix for now
#include <unistd.h>

/* our backoff timer doesn't count down in idle times during a
 * frame-exchange sequence as the mac tx state isn't idle; genreally
 * these idle times are less than DIFS and won't contribute to
 * counting down the backoff period, but this could be a real
 * problem if the frame exchange ends up in a timeout! in that case,
 * i.e. if a timeout happens we've not been counting down for the
 * duration of the timeout, and in fact begin counting down only
 * DIFS after the timeout!! we lose the timeout interval - which
 * will is not the REAL case! also, the backoff timer could be NULL
 * and we could have a pending transmission which we could have
 * sent! one could argue this is an implementation artifact which
 * doesn't violate the spec.. and the timeout interval is expected
 * to be less than DIFS .. which means its not a lot of time we
 * lose.. anyway if everyone hears everyone the only reason a ack will
 * be delayed will be due to a collision => the medium won't really be
 * idle for a DIFS for this to really matter!!
 */

inline void
Mac802_11::checkBackoffTimer()
{

	if(is_idle() && mhBackoff_.paused()) { 

		#ifdef DEBUG
		fprintf(stdout, "%f \tNode:%d in checkBackoffTimer, and medium is idle\n", Scheduler::instance().clock(), index_);
		#endif

		mhBackoff_.resume(phymib_.getDIFS());

	}
	if(! is_idle() && mhBackoff_.busy() && ! mhBackoff_.paused()) {

		#ifdef DEBUG
		fprintf(stdout, "%f \tNode:%d in checkBackoffTimer, and I am backing off\n", Scheduler::instance().clock(), index_);
		#endif

		mhBackoff_.pause();

	}
}

inline void
Mac802_11::transmit(Packet *p, double timeout)
{
	tx_active_ = 1;

//duy
//for debugging purposes
	hdr_mac802_11* mh = HDR_MAC802_11(p);
	int ch = (int)mh->dh_fc.fc_channel;
        int dst = (int) ETHER_ADDR(mh->dh_ra);
	char my_type[80];
	switch(mh->dh_fc.fc_type) {
	case MAC_Type_Management:
		strcpy(my_type, "Manage");
		break;
	case MAC_Type_Control:
		switch(mh->dh_fc.fc_subtype) {
			case MAC_Subtype_RTS:
				strcpy(my_type, "RTS");
				break;
			case MAC_Subtype_CTS:
				strcpy(my_type, "CTS");
				break;
			case MAC_Subtype_ATS:
				strcpy(my_type, "ATS");
				break;
			case MAC_Subtype_ACK:
				strcpy(my_type, "ACK");
				break;
			default:
				strcpy(my_type, "CTRLSOS");
				break;
			}
		break;
	case MAC_Type_Data:
		strcpy(my_type, "DATA");
		set_stop_watch(Scheduler::instance().clock());
		break;

	default:
		strcpy(my_type, "SOS");
		break;
	}

	#ifdef DEBUG2
	fprintf(stdout, "%f \tNode:%d Transmitting >>>>>>>>>>> a packet %s \ton channel %d to Node:%d\n", Scheduler::instance().clock(), index_, my_type, ch, dst);
	#endif

// end of duy

	if (EOTtarget_) {
		assert (eotPacket_ == NULL);
		//duy comments: copy for eot callback
		eotPacket_ = p->copy();
	}

	/*
	 * If I'm transmitting without doing CS, such as when
	 * sending an ACK, any incoming packet will be "missed"
	 * and hence, must be discarded.
	 */
	 //ddn pktRx_ see mac.h
	if(rx_state_ != MAC_IDLE) {
		//assert(dh->dh_fc.fc_type == MAC_Type_Control);
		//assert(dh->dh_fc.fc_subtype == MAC_Subtype_ACK);
		assert(pktRx_);
		struct hdr_cmn *ch = HDR_CMN(pktRx_);
		ch->error() = 1;        /* force packet discard */
	}

	/*
	 * pass the packet on the "interface" which will in turn
	 * place the packet on the channel.
	 *
	 * NOTE: a handler is passed along so that the Network
	 *       Interface can distinguish between incoming and
	 *       outgoing packets.
	 */
	downtarget_->recv(p->copy(), this);	
	mhSend_.start(timeout);
	mhIF_.start(txtime(p));
}


inline void
Mac802_11::setRxState(MacState newState)
{

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d Changing RxState to %s\n", Scheduler::instance().clock(), index_, convertState(newState));
	#endif

	rx_state_ = newState;
	checkBackoffTimer();
}

inline void
Mac802_11::setTxState(MacState newState)
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d Changing TxState to %s\n", Scheduler::instance().clock(), index_, convertState(newState));
	#endif

	tx_state_ = newState;
	checkBackoffTimer();
}

//duy
//for debugging purposes
char *
Mac802_11::convertState( MacState newState) {

	if(newState == MAC_IDLE)
		return "MAC_IDLE";
	else if(newState == MAC_POLLING) 
		return "MAC_POLLING";
	else if(newState == MAC_RECV) 
		return "MAC_RECV";
	else if(newState == MAC_SEND) 
		return "MAC_SEND";
	else if(newState == MAC_RTS) 
		return "MAC_RTS";
	else if(newState == MAC_CTS) 
		return "MAC_CTS";
	else if(newState == MAC_WAIT) 
		return "MAC_WAIT";
	else if(newState == MAC_ATS) 
		return "MAC_ATS";
	else if(newState == MAC_ACK) 
		return "MAC_ACK";
	else if(newState == MAC_COLL) 
		return "MAC_COLL";

	return "ERROR";

}
//end of duy

/* ======================================================================
   TCL Hooks for the simulator
   ====================================================================== */
static class Mac802_11Class : public TclClass {
public:
	Mac802_11Class() : TclClass("Mac/802_11") {}
	TclObject* create(int, const char*const*) {
	return (new Mac802_11());

}
} class_mac802_11;


/* ======================================================================
   Mac  and Phy MIB Class Functions
   ====================================================================== */

PHY_MIB::PHY_MIB(Mac802_11 *parent)
{
	/*
	 * Bind the phy mib objects.  Note that these will be bound
	 * to Mac/802_11 variables
	 */

	parent->bind("CWMin_", &CWMin);
	parent->bind("CWMax_", &CWMax);
	parent->bind("SlotTime_", &SlotTime);
	parent->bind("SIFS_", &SIFSTime);
	parent->bind("PreambleLength_", &PreambleLength);
	parent->bind("PLCPHeaderLength_", &PLCPHeaderLength);
	parent->bind_bw("PLCPDataRate_", &PLCPDataRate);
}

MAC_MIB::MAC_MIB(Mac802_11 *parent)
{
	/*
	 * Bind the phy mib objects.  Note that these will be bound
	 * to Mac/802_11 variables
	 */
	
	parent->bind("RTSThreshold_", &RTSThreshold);
	parent->bind("ShortRetryLimit_", &ShortRetryLimit);
	parent->bind("LongRetryLimit_", &LongRetryLimit);
}

/* ======================================================================
   Mac Class Functions
   ====================================================================== */
Mac802_11::Mac802_11() : 
	Mac(), phymib_(this), macmib_(this), mhIF_(this), mhNav_(this), 
	mhRecv_(this), mhSend_(this), 
	mhDefer_(this), mhBackoff_(this),
	mhWait_(this), mhRdezvous_(this), mhCtrl_(this) //duy

{
	
	//duy

	for( int i=0; i<= NUM_OF_CHANNELS;i++)
	{
		nav_ = 0.0;
		ammac_cul_[i] = 0.0;
		ammac_cul_node_[i][0] = -1;
		ammac_cul_node_[i][1] = -1;
		ammac_recved_fcl_[i] = 0;
	}

	ammac_ctrl_time_ = 0;
	ammac_data_time_ = 0;
	ammac_cur_state_ = AMMAC_IDLE;
	ammac_cur_dest_ = -1;

	selected_channel_ = -1;

	active_channel_ = DEFAULT_CHANNEL;
	past_active_channel_ = -1;

	//end of duy

	tx_state_ = rx_state_ = MAC_IDLE;
	tx_active_ = 0;
	eotPacket_ = NULL;

	pktRTS_ = 0;

	pktCTRL_ = 0;		

	pktATS_ = 0; //duy

	cw_ = phymib_.getCWMin();
	ssrc_ = slrc_ = 0;  //station short retry and long retry count
	// Added by Sushmita
        et_ = new EventTrace();
	
	sta_seqno_ = 1;
	cache_ = 0;
	cache_node_count_ = 0;
	
	// chk if basic/data rates are set
	// otherwise use bandwidth_ as default;
	
	Tcl& tcl = Tcl::instance();
	tcl.evalf("Mac/802_11 set basicRate_");
	if (strcmp(tcl.result(), "0") != 0) 
		bind_bw("basicRate_", &basicRate_);
	else
		basicRate_ = bandwidth_;

	tcl.evalf("Mac/802_11 set dataRate_");
	if (strcmp(tcl.result(), "0") != 0) 
		bind_bw("dataRate_", &dataRate_);
	else
		dataRate_ = bandwidth_;

	bind_bool("bugFix_timer_", &bugFix_timer_);

        EOTtarget_ = 0;
       	bss_id_ = IBSS_ID;
	//printf("bssid in constructor %d\n",bss_id_);

}


int
Mac802_11::command(int argc, const char*const* argv)
{
	if (argc == 3) {
		if (strcmp(argv[1], "eot-target") == 0) {
			EOTtarget_ = (NsObject*) TclObject::lookup(argv[2]);
			if (EOTtarget_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else if (strcmp(argv[1], "bss_id") == 0) {
			bss_id_ = atoi(argv[2]);
			return TCL_OK;
		} else if (strcmp(argv[1], "log-target") == 0) { 
			logtarget_ = (NsObject*) TclObject::lookup(argv[2]);
			if(logtarget_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else if(strcmp(argv[1], "nodes") == 0) {
			if(cache_) return TCL_ERROR;
			cache_node_count_ = atoi(argv[2]);
			cache_ = new Host[cache_node_count_ + 1];
			assert(cache_);
			bzero(cache_, sizeof(Host) * (cache_node_count_+1 ));
			return TCL_OK;
		} else if(strcmp(argv[1], "eventtrace") == 0) {
			// command added to support event tracing by Sushmita
                        et_ = (EventTrace *)TclObject::lookup(argv[2]);
                        return (TCL_OK);
                }
	}
	return Mac::command(argc, argv);
}

// Added by Sushmita to support event tracing
void Mac802_11::trace_event(char *eventtype, Packet *p) 
{
        if (et_ == NULL) return;
        char *wrk = et_->buffer();
        char *nwrk = et_->nbuffer();
	
        //char *src_nodeaddr =
	//       Address::instance().print_nodeaddr(iph->saddr());
        //char *dst_nodeaddr =
        //      Address::instance().print_nodeaddr(iph->daddr());
	
        struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	
        //struct hdr_cmn *ch = HDR_CMN(p);
	
	if(wrk != 0) {
		sprintf(wrk, "E -t "TIME_FORMAT" %s %2x ",
			et_->round(Scheduler::instance().clock()),
                        eventtype,
                        //ETHER_ADDR(dh->dh_sa)
                        ETHER_ADDR(dh->dh_ta)
                        );
        }
        if(nwrk != 0) {
                sprintf(nwrk, "E -t "TIME_FORMAT" %s %2x ",
                        et_->round(Scheduler::instance().clock()),
                        eventtype,
                        //ETHER_ADDR(dh->dh_sa)
                        ETHER_ADDR(dh->dh_ta)
                        );
        }
        et_->dump();
}

/* ======================================================================
   Debugging Routines
   ====================================================================== */
void
Mac802_11::trace_pkt(Packet *p) 
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	u_int16_t *t = (u_int16_t*) &dh->dh_fc;

	fprintf(stderr, "\t[ %2x %2x %2x %2x ] %x %s %d\n",
		*t, dh->dh_duration,
		 ETHER_ADDR(dh->dh_ra), ETHER_ADDR(dh->dh_ta),
		index_, packet_info.name(ch->ptype()), ch->size());
}

void
Mac802_11::dump(char *fname)
{
	fprintf(stderr,
		"\n%s --- (INDEX: %d, time: %2.9f)\n",
		fname, index_, Scheduler::instance().clock());
		
	fprintf(stderr,
		"\ttx_state_: %x, rx_state_: %x, nav: %2.9f, idle: %d\n",
		tx_state_, rx_state_, nav_, is_idle());

	fprintf(stderr,
		"\tpktTx_: %lx, pktRx_: %lx, pktRTS_: %lx, pktCTRL_: %lx, callback: %lx\n",
		(long) pktTx_, (long) pktRx_, (long) pktRTS_,
		(long) pktCTRL_, (long) callback_);

	fprintf(stderr,
		"\tDefer: %d, Backoff: %d (%d), Recv: %d, Timer: %d Nav: %d\n",
		mhDefer_.busy(), mhBackoff_.busy(), mhBackoff_.paused(),
		mhRecv_.busy(), mhSend_.busy(), mhNav_.busy());
	fprintf(stderr,
		"\tBackoff Expire: %f\n",
		mhBackoff_.expire());
}


/* ======================================================================
   Packet Headers Routines
   ====================================================================== */
inline int
Mac802_11::hdr_dst(char* hdr, int dst )
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
	
       if (dst > -2) {
               if ((bss_id() == ((int)IBSS_ID)) || (addr() == bss_id())) {
                       /* if I'm AP (2nd condition above!), the dh_3a
                        * is already set by the MAC whilst fwding; if
                        * locally originated pkt, it might make sense
                        * to set the dh_3a to myself here! don't know
                        * how to distinguish between the two here - and
                        * the info is not critical to the dst station
                        * anyway!
                        */
                       STORE4BYTE(&dst, (dh->dh_ra));
               } else {
                       /* in BSS mode, the AP forwards everything;
                        * therefore, the real dest goes in the 3rd
                        * address, and the AP address goes in the
                        * destination address
                        */
                       STORE4BYTE(&bss_id_, (dh->dh_ra));
                       STORE4BYTE(&dst, (dh->dh_3a));
               }
       }


       return (u_int32_t)ETHER_ADDR(dh->dh_ra);
}

inline int 
Mac802_11::hdr_src(char* hdr, int src )
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
        if(src > -2)
               STORE4BYTE(&src, (dh->dh_ta));
        return ETHER_ADDR(dh->dh_ta);
}

inline int 
Mac802_11::hdr_type(char* hdr, u_int16_t type)
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
	if(type)
		STORE2BYTE(&type,(dh->dh_body));
	return GET2BYTE(dh->dh_body);
}


/* ======================================================================
   Misc Routines
   ====================================================================== */
	
inline int
Mac802_11::is_idle()
{
	if(rx_state_ != MAC_IDLE)
		return 0;
	if(tx_state_ != MAC_IDLE)
		return 0;
	if(nav_ > Scheduler::instance().clock())
		return 0;
	
	return 1;
}

void
Mac802_11::discard(Packet *p, const char* why)
{
	hdr_mac802_11* mh = HDR_MAC802_11(p);
	hdr_cmn *ch = HDR_CMN(p);

#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in discard pkt src %d pkt dest %d reason is %s\n", Scheduler::instance().clock(), index_, (u_int32_t)ETHER_ADDR(mh->dh_ta), (u_int32_t)ETHER_ADDR(mh->dh_ra), why);
#endif

	/* if the rcvd pkt contains errors, a real MAC layer couldn't
	   necessarily read any data from it, so we just toss it now */
	if(ch->error() != 0) {
		Packet::free(p);
		return;
	}

	switch(mh->dh_fc.fc_type) {
	case MAC_Type_Management:
		drop(p, why);
		return;
	case MAC_Type_Control:
		switch(mh->dh_fc.fc_subtype) {
		case MAC_Subtype_RTS:
			 if((u_int32_t)ETHER_ADDR(mh->dh_ta) ==  (u_int32_t)index_) {
				drop(p, why);
				return;
			}
			/* fall through - if necessary */
		case MAC_Subtype_CTS:
		case MAC_Subtype_ATS:
		case MAC_Subtype_ACK:
			if((u_int32_t)ETHER_ADDR(mh->dh_ra) == (u_int32_t)index_) {
				drop(p, why);
				return;
			}
			break;
		default:
			fprintf(stderr, "invalid MAC Control subtype\n");
			exit(1);
		}
		break;
	case MAC_Type_Data:
		switch(mh->dh_fc.fc_subtype) {
		case MAC_Subtype_Data:
			if((u_int32_t)ETHER_ADDR(mh->dh_ra) == \
                           (u_int32_t)index_ ||
                          (u_int32_t)ETHER_ADDR(mh->dh_ta) == \
                           (u_int32_t)index_ ||
                          (u_int32_t)ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
                                drop(p,why);
                                return;
			}
			break;
		default:
			fprintf(stderr, "invalid MAC Data subtype\n");
			exit(1);
		}
		break;
	default:
		fprintf(stderr, "invalid MAC type (%x)\n", mh->dh_fc.fc_type);
		trace_pkt(p);
		exit(1);
	}
	Packet::free(p);
}

void
Mac802_11::capture(Packet *p)
{

	struct hdr_mac802_11*dh = HDR_MAC802_11(p);
	int ch = (int)dh->dh_fc.fc_channel;
	
	/*
	 * Update the NAV so that this does not screw
	 * up carrier sense.
	 */	
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in capture channel is\n", Scheduler::instance().clock(), index_, ch);
	#endif
	 
	if(ch == 0) {
		set_nav(usec(phymib_.getEIFS() + txtime(p)));
	}
	Packet::free(p);
}

void
Mac802_11::collision(Packet *p)
{
	struct hdr_mac802_11*dh = HDR_MAC802_11(p);
	int ch = (int)dh->dh_fc.fc_channel;

	int dst = (int) ETHER_ADDR(dh->dh_ra);
	int src = (int) ETHER_ADDR(dh->dh_ta);

	#ifdef DEBUG2
	fprintf(stdout, "%f \tNode:%d is in collision, my active_chan: %d selectedchan: %d packetchan: %d node sender %d intend for %d\n", Scheduler::instance().clock(), index_, active_channel_, selected_channel_, ch,src, dst);

	for(int i= NUM_OF_CHANNELS-1; i>=0; i--)
		fprintf(stdout, "%f \t***channel time usage %f by the pair %d and %d on channel %d\n", Scheduler::instance().clock(), ammac_cul_[i], ammac_cul_node_[i][0],ammac_cul_node_[i][1], i);
	#endif

	switch(rx_state_) {
	case MAC_RECV:
		setRxState(MAC_COLL);
		/* fall through */
	case MAC_COLL:
		assert(pktRx_);
		assert(mhRecv_.busy());
		/*
		 *  Since a collision has occurred, figure out
		 *  which packet that caused the collision will
		 *  "last" the longest.  Make this packet,
		 *  pktRx_ and reset the Recv Timer if necessary.
		 */
		if(txtime(p) > mhRecv_.expire()) {
			mhRecv_.stop();
			discard(pktRx_, DROP_MAC_COLLISION);
			pktRx_ = p;
			mhRecv_.start(txtime(pktRx_));
		}
		else {
			discard(p, DROP_MAC_COLLISION);
		}
		break;
	default:
		assert(0);
	}
}

// duy comments:
//called by send_timer(), recvRTS(), recvCTS(), recvDATA(), recvACK()
void
Mac802_11::tx_resume()
{
	double rTime;
	assert(mhSend_.busy() == 0);
	assert(mhDefer_.busy() == 0);
	
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in tx_resume \n", Scheduler::instance().clock(), index_);
	#endif


	//duy comments: say a control packet is being held means 
	//a CTS or ACK needs to be sent
	if(pktCTRL_) {
		#ifdef DEBUG
		printf("in tx_resume() it is pktCtrl_\n");
		#endif
		/*
		 *  Need to send a CTS or ACK.
		 */
		mhDefer_.start(phymib_.getSIFS());
	} else if(pktATS_) {
		#ifdef DEBUG
		printf("in tx_resume it is pktATS_\n");
		#endif
		mhDefer_.start(phymib_.getSIFS());

	}
	//duy comments: RTS pkt is held and backoff timer is not set
	//means that RTS has been sent out and expire, a retransmission may be needed
	//set contention window to defer time
	else if(pktRTS_) {
		#ifdef DEBUG
		printf("in tx_resume it is pktRTS_\n");
		#endif
		if ((mhBackoff_.busy() == 0) && (mhWait_.busy() == 0)) {
			if (bugFix_timer_) {
				mhBackoff_.start(cw_, is_idle(), 
						 phymib_.getDIFS());
			}
			else {
				rTime = (Random::random() % cw_) * 
					phymib_.getSlotTime();
				mhDefer_.start( phymib_.getDIFS() + rTime);
			}
		}
	}
	//duy comments: a packet needed to be sent(from higher layer such as data pkt, ARP pkt
	//routing control pkt etc.), previous tries have failed, a retransmission is needed
	//set defer to a new contention window
	 else if(pktTx_) {
		#ifdef DEBUG
		printf("in tx_resume it is pktTx_\n");
		#endif
		if (mhBackoff_.busy() == 0) {
			hdr_cmn *ch = HDR_CMN(pktTx_);
			struct hdr_mac802_11 *mh = HDR_MAC802_11(pktTx_);
			
			if ((u_int32_t) ch->size() < macmib_.getRTSThreshold()
			    || (u_int32_t) ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
				if (bugFix_timer_) {
					mhBackoff_.start(cw_, is_idle(), 
							 phymib_.getDIFS());
				}
				else {
					rTime = (Random::random() % cw_)
						* phymib_.getSlotTime();
					mhDefer_.start(phymib_.getDIFS() + 
						       rTime);
				}
                        } else {
				mhDefer_.start(phymib_.getSIFS());
                        }
		}

	//duy comments: every thing fails, means no packet is waiting to be sent
	} else if(callback_) {
		Handler *h = callback_;
		callback_ = 0;
		h->handle((Event*) 0);
	}
	if(tx_state_ != MAC_WAIT)
		setTxState(MAC_IDLE);
}


//duy comments: 
//called by recv_timer(), setting receiving state to MAC_IDLE
void
Mac802_11::rx_resume()
{
	assert(pktRx_ == 0);
	assert(mhRecv_.busy() == 0);
	setRxState(MAC_IDLE);
}


/* ======================================================================
   Timer Handler Routines
   ====================================================================== */
void
Mac802_11::backoffHandler()
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in backoffHandler\n", Scheduler::instance().clock(), index_);
	#endif

	if(pktCTRL_) {
		assert(mhSend_.busy() || mhDefer_.busy());
		return;
	}

	if(check_pktRTS() == 0)
		return;

	if(check_pktTx() == 0)
		return;
}



//duy comments:
//checking if there is a pkt waiting to be sent by calling
//check_pktCTRL() check_pktATS() check_pktRTS check_pktTX
void
Mac802_11::deferHandler()
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in deferHandler\n", Scheduler::instance().clock(), index_);
	#endif

	assert(pktCTRL_ || pktRTS_ || pktTx_);

	if(check_pktCTRL() == 0)
		return;
	if(check_pktATS() == 0)
		return;
	assert(mhBackoff_.busy() == 0);
	if(check_pktRTS() == 0)
		return;
	if(check_pktTx() == 0)
		return;
}

void
Mac802_11::navHandler()
{
	if(is_idle() && mhBackoff_.paused())
		mhBackoff_.resume(phymib_.getDIFS());
}

void
Mac802_11::recvHandler()
{
	recv_timer();
}

//duy
void
Mac802_11::waitHandler()
{

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in waitHandler, state=%d\n", NOW, index_, ammac_cur_state_);
	#endif
	if(ammac_cur_state_ == AMMAC_IDLE)
	{
		if(mhBackoff_.busy() == 0)
			mhBackoff_.start(cw_, is_idle());
	}
}
void
Mac802_11::ctrlHandler(int dummy)
{
	printf("ctrl expires\n");
	ammac_ctrl_time_ = 0.0;
	if(ammac_cur_state_ == AMMAC_CTRL_WIN)
		ammac_cur_state_ = AMMAC_IDLE;

}

//this handler is associated with ATS packet
void
Mac802_11::rdezvousHandler()
{

	ammac_data_time_ = get_data_transfer() + NOW; 

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d	rdezvousHandler() at ammac state %d (0=idle, 1=ctrl, 2=data) selected channel is %d\n", Scheduler::instance().clock(), index_, ammac_cur_state_, selected_channel_);
	#endif

	double data_time;
	double rTime;

	switch(ammac_cur_state_) {

	case AMMAC_CTRL_WIN:
		ammac_cur_state_ = AMMAC_DATA_WIN;
			


		data_time =  get_data_transfer() + phymib_.getSIFS();

		#ifdef DEBUG
		fprintf(stdout,"data time rdezvousHandler %f\n", data_time + NOW);
		#endif
		if(mhRdezvous_.busy())
			mhRdezvous_.stop();
		mhRdezvous_.start(data_time);

		if(selected_channel_ >= 0) {
			active_channel_ = selected_channel_;
			past_active_channel_ = active_channel_;
		}
		else
			active_channel_ = DEFAULT_CHANNEL;

		//if(active_channel_ != DEFAULT_CHANNEL) 
		//	ammacClearChannelState();

		assert(pktTx_);
		

		//this will do too
		//	if(pktTx_ == 0)
		//		return;

		break;
	case AMMAC_DATA_WIN:

		fprintf(stdout, "in rdezvousHandler %f\n", mhWait_.expire());
		if(mhWait_.busy());
			mhWait_.stop();

		if(active_channel_ != DEFAULT_CHANNEL) 
			ammacClearChannelState();

		ammac_cur_state_ = AMMAC_IDLE;
		active_channel_ = DEFAULT_CHANNEL;
		selected_channel_ = -1;

		if(mhDefer_.busy())
			mhDefer_.stop();

	//either this or below	
		if(mhBackoff_.busy() == 0)
			mhBackoff_.start(cw_, is_idle());

		/*
		//to avoid collisions
		if(mhBackoff_.busy() == 0) {
			if(is_idle()) {
				if (mhDefer_.busy() == 0) {
					rTime = (Random::random() % cw_)
						* (phymib_.getSlotTime());
					mhDefer_.start(phymib_.getDIFS() + 
					       		rTime);
				}
			} else {
				if(past_active_channel_ != 0) {
		 			past_active_channel_ = -1;
				} 
				else
					rst_cw();
				mhBackoff_.start(cw_, is_idle());

				#ifdef DEBUG
				printf("ddn not idle\n");
				#endif

			}
		} else {

			//tomorrow do this
			//i think set_nav is it
			//checkBackoffTimer();
			//set_nav(usec(phymib_.getDIFS()));
		

		}
		*/
		
		//ok problem here
		fprintf(stdout, "difs is %f eifs is %f\n", phymib_.getDIFS(), phymib_.getEIFS());
		set_nav(usec(2*phymib_.getDIFS()));
		ammac_data_time_ = 0; 
		break;

	case AMMAC_IDLE:
		printf("ERROR - rdezvoushandler in AMMAC_IDLE \n");
	default:
		printf("ERROR - rdezvoushandler in bad state \n");
		exit(1);
		break;
	}
}

//end of duy


void
Mac802_11::sendHandler()
{
	send_timer();
}


void
Mac802_11::txHandler()
{
	if (EOTtarget_) {
		assert(eotPacket_);
		EOTtarget_->recv(eotPacket_, (Handler *) 0);
		eotPacket_ = NULL;
	}
	tx_active_ = 0;
}


/* ======================================================================
   The "real" Timer Handler Routines
   ====================================================================== */
//duy comments
//the send timer would be set by transmit(); when send timer expires,
//this function would check the transmission state tx_state_
void
Mac802_11::send_timer()
{
	switch(tx_state_) {

	/*
	 * Sent a RTS, but did not receive a CTS.
	 */
	case MAC_RTS:
		RetransmitRTS();
		break;

//duy	
	/*
	 * Sent a CTS, but did not receive a ATS packet.
	 */
	case MAC_CTS:
		assert(pktCTRL_);
		Packet::free(pktCTRL_); 
		pktCTRL_ = 0;
		selected_channel_ = -1;
		break;
	/*
	 * Sent a ATS, now ready for rendezvous 
	 */
	case MAC_ATS:
		assert(pktATS_);
		Packet::free(pktATS_);
		pktATS_ = 0;
		
		if(mhRdezvous_.busy() )	
			mhRdezvous_.stop();

		if(mhWait_.busy())
			mhWait_.stop();

		//ammac_data_time_ = get_data_transfer() + NOW;
		rdezvousHandler();


		break;

	case MAC_WAIT:
		setTxState(MAC_IDLE);
		break;
//end of duy
			
	/*
	 * Sent DATA, but did not receive an ACK packet.
	 */
	case MAC_SEND:
		//you may only retransmit if you're on channel 0
		//may have to do something
		RetransmitDATA();
		break;
	/*
	 * Sent an ACK, and now ready to resume transmission.
	 */
	case MAC_ACK:
		assert(pktCTRL_);
		Packet::free(pktCTRL_); 
		pktCTRL_ = 0;
		break;
	case MAC_IDLE:
		break;
	default:
		assert(0);
	}
	tx_resume();
}


/* ======================================================================
   Outgoing Packet Routines
   ====================================================================== */

//duy comments:
//checking pkt held in pktCTRL_
//return 0 if a control pkt is held by this field, -1 otherwise
//two types: CTS and ACK
//if it's CTS, sets transmission state to MAC_CTS through setTxState()
//calculating transmitting time of CTS and calling transmit to place CTS on channel
//if it's ACK, calculate transmission time of an ACK and call transmit to send this ACK pkt
//sets transmission state to MAC_CTS or MAC_ACK respectively
int
Mac802_11::check_pktCTRL()
{
	struct hdr_mac802_11 *mh;
	double timeout, time_left, total_time, allow_data_time;
	struct cts_frame * cf; 

	if(pktCTRL_ == 0)
		return -1;
	if(tx_state_ == MAC_CTS || tx_state_ == MAC_ACK)
		return -1;

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in check_pktCTRL() active_channel is %d\n", Scheduler::instance().clock(), index_, active_channel_);
	#endif


	mh = HDR_MAC802_11(pktCTRL_);

							  
	switch(mh->dh_fc.fc_subtype) {
	/*
	 *  If the medium is not IDLE, don't send the CTS.
	 */
	case MAC_Subtype_CTS:

		cf = (struct cts_frame*) pktCTRL_->access(hdr_mac::offset_);
		if(!is_idle()) {

			#ifdef DEBUG
			fprintf(stdout, "%f \tNode:%d in chk_pktCTS: medium is busy\n", Scheduler::instance().clock(), index_);
			#endif

			discard(pktCTRL_, DROP_MAC_BUSY); pktCTRL_ = 0;
			return 0;
		}
//duy
		//check states after CTS is cleared
		//getting set for transmission
		switch (ammac_cur_state_) {
		
		case AMMAC_IDLE:
			
			selected_channel_ = ammacSelectChannel();
			ammac_cur_state_ = AMMAC_CTRL_WIN;
			
			if(selected_channel_ == 0) {
				allow_data_time = ammacFollowInitiator();
				if(allow_data_time == 0.0) return 0;
				cf->cf_duration_data_win = usec_32(allow_data_time);
				fprintf(stdout, " WWW %f %f\n", allow_data_time, sec_uint(cf->cf_duration_data_win));
			}

			cf->cf_selected_channel = selected_channel_;
//			cf->cf_duration_ctrl_win = usec_32(ammac_ctrl_time_ - NOW);
			cf->cf_duration_ctrl_win = usec_32(mhCtrl_.expire());

			break;

		case AMMAC_CTRL_WIN:
			
//			time_left = ammac_ctrl_time_ - NOW;
			time_left = mhCtrl_.expire();

			total_time = txtime(phymib_.getCTSlen(), basicRate_)
					+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
					+ txtime(phymib_.getATSlen(), basicRate_)
					+ DSSS_MaxPropagationDelay;
			#ifdef DEBUG
			fprintf(stdout, "time left is %f, total time is %f\n", time_left, total_time);
			#endif

			if( time_left < total_time)
			{

				#ifdef DEBUG
				printf("in check_pktCTRL, not enough time left for one more negotiation\n");
				#endif

				ammacClearVars();
				
				discard(pktCTRL_, DROP_MAC_BUSY);
				pktCTRL_ = 0;
				return 0;
			}

			else {

				selected_channel_ = ammacSelectChannel();

				if(selected_channel_ == 0) {
					allow_data_time = ammacFollowInitiator();
					if(allow_data_time == 0.0) return 0;
					cf->cf_duration_data_win = usec_32(allow_data_time);
					fprintf(stdout, " WWW %f  %f\n", allow_data_time, sec_uint(cf->cf_duration_data_win));
				}
				#ifdef DEBUG
				fprintf(stdout, "%f \tNode:%d, in check_pktCTRL enough time left for %f negotiation(s)\n selected chan is %d\n", Scheduler::instance().clock(), index_, (time_left/(total_time + txtime(phymib_.getRTSlen(), basicRate_) + DSSS_MaxPropagationDelay+phymib_.getSIFS())),selected_channel_);
				#endif

				cf->cf_selected_channel = selected_channel_;
//				cf->cf_duration_ctrl_win = usec_32(ammac_ctrl_time_ - NOW);
				cf->cf_duration_ctrl_win = usec_32(mhCtrl_.expire());
			}	
			break;

		case AMMAC_DATA_WIN:
			discard(pktCTRL_, DROP_MAC_BUSY); pktCTRL_ =0;
			return 0;
			break;
		default:
			printf("ERROR: non-ammac state in check pkt RTS\n");
		}
		cf->cf_fc.fc_channel = DEFAULT_CHANNEL;

		setTxState(MAC_CTS);

		timeout = txtime(phymib_.getCTSlen(), basicRate_) + phymib_.getSIFS()
				+ DSSS_MaxPropagationDelay 
				+ txtime(phymib_.getATSlen(), basicRate_) + phymib_.getSIFS()
				+ DSSS_MaxPropagationDelay;
		if(selected_channel_ < 0)
			timeout = txtime(phymib_.getCTSlen(), basicRate_);
//end of duy
		break;

	case MAC_Subtype_ACK:		
		setTxState(MAC_ACK);
		timeout = txtime(phymib_.getACKlen(), basicRate_);
		break;
	default:
		fprintf(stderr, "check_pktCTRL:Invalid MAC Control subtype\n");
		exit(1);
	}

	transmit(pktCTRL_, timeout);
	return 0;
}

//duy
void
Mac802_11::ammacClearVars()
{
	ammac_cur_state_ = AMMAC_IDLE;
	ammac_ctrl_time_ = 0.0;
	ammac_data_time_ = 0.0;
//	if(mhRdezvous_.busy())
//		mhRdezvous_.stop();
	if(mhCtrl_.busy())
		mhCtrl_.stop();
}


int
Mac802_11::check_pktATS()
{
	struct hdr_mac802_11 *mh;
	double timeout, total_time, time_left, temp;

	assert(mhBackoff_.busy() == 0);

	if(pktATS_ == 0)
		return -1;
	if(tx_state_ == MAC_ATS)
		return -1;
	
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in check_pktATS\n", Scheduler::instance().clock(), index_);
	#endif

	struct ats_frame *rf = (struct ats_frame*) pktATS_->access(hdr_mac::offset_);


	mh = HDR_MAC802_11(pktATS_);

	switch(mh->dh_fc.fc_subtype) {
	
	case MAC_Subtype_ATS:
		if(!is_idle()) {
			
			selected_channel_ = -1;
			sendRTS((int)ETHER_ADDR(mh->dh_ra));
			if(mhBackoff_.busy() == 0) {
				inc_cw();
				mhBackoff_.start(cw_, is_idle());
			}
			discard(pktATS_, DROP_MAC_BUSY); pktATS_ = 0;
			return 0;
		}
		switch(ammac_cur_state_) {

		case AMMAC_DATA_WIN:
			
			discard(pktATS_, DROP_MAC_BUSY); pktATS_ = 0;
			return 0;
			break;

		case AMMAC_CTRL_WIN:
//			time_left = ammac_ctrl_time_ - NOW;
			time_left = mhCtrl_.expire(); 

			total_time = txtime(phymib_.getATSlen(), basicRate_) 
					+ DSSS_MaxPropagationDelay ;

			if(time_left < total_time) {
				#ifdef DEBUG
				printf("in check_pktATS, not enough time left \n");
				#endif

				ammacClearVars();
				discard(pktATS_, DROP_MAC_BUSY); pktATS_ = 0;
				return 0;
			}
			else {
				
				temp = txtime(phymib_.getRTSlen(), basicRate_)
					+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
					+ txtime(phymib_.getCTSlen(), basicRate_)
					+ DSSS_MaxPropagationDelay + phymib_.getSIFS();
				#ifdef DEBUG
				fprintf(stdout, "%f \tNode:%d,  in check_pktATS enough time left for %f negotiation(s)\n", Scheduler::instance().clock(), index_, (time_left/(total_time + temp)));
				#endif

				rf->ats_confirmed_channel = selected_channel_;
//				rf->ats_duration_ctrl_win = usec_32(ammac_ctrl_time_ - NOW);
				rf->ats_duration_ctrl_win = usec_32(mhCtrl_.expire());
			}
			break;

		case AMMAC_IDLE:
			//shouldn't be here
			#ifdef DEBUG
			printf("check_pktATS is idle???\n");
			#endif
			rf->ats_confirmed_channel = selected_channel_;
//			rf->ats_duration_ctrl_win = usec_32(ammac_ctrl_time_ - NOW);
			rf->ats_duration_ctrl_win = usec_32(mhCtrl_.expire());
			break;
		default:
			printf("ERROR: non-ammac state in check pkt ATS\n");
			exit(1);
		}

		setTxState(MAC_ATS);
		timeout = txtime(pktATS_);
		break;
	default:
		fprintf(stderr, "check_pktATS: Invalid MAC control subtype\n");
		exit(1);
	}

	transmit(pktATS_, timeout);
	return 0;
}


//end of duy
			

//duy comments
//check pktRTS_ holding RTS pkt, return 0 if it does, otherwise -1
//if RTS is held
//Either send it using transmit, setting transmissions state to MAC_RTS
//OR if channel is not idle, backoff timer would be set as well as
//increasing contention window size
int
Mac802_11::check_pktRTS()
{

	//try to make this global or constant
//	double data_limit =  .002630;
//	double data_limit =  .010000;
	double data_limit = 0; 

	struct hdr_mac802_11 *mh;
	double timeout, time_left, total_time, padding, commonChan, my_data, temp_data, free_time, remain, added_time, chan_stats,one_nego;

	double delay = DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getACKlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;

	data_limit;

	one_nego = txtime(phymib_.getRTSlen(), basicRate_)
				+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getCTSlen(), basicRate_)
				+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getATSlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;
	padding = txtime(phymib_.getRTSlen(), basicRate_)
				+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getCTSlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;// + phymib_.getSIFS();
				//+ phymib_.getDIFS();// + phymib_.getDIFS();

	assert(mhBackoff_.busy() == 0);

	if(pktRTS_ == 0)
 		return -1;


	struct rts_frame * rf = (struct rts_frame*) pktRTS_->access(hdr_mac::offset_);

	mh = HDR_MAC802_11(pktRTS_);
	int dst = (int)ETHER_ADDR(mh->dh_ra);


	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in check_pktRTS for dst node  %d ammac_ctrl_time is %f\n", Scheduler::instance().clock(), index_, dst,mhCtrl_.expire() + NOW);
	//fprintf(stdout, "%f \tNode:%d in check_pktRTS for dst node  %d data transfer is %f\n", Scheduler::instance().clock(), index_, dst,get_data_transfer());
	fprintf(stdout, "%f \tNode:%d in check_pktRTS for dst node  %d data time is %f\n", Scheduler::instance().clock(), index_, dst,txtime(pktTx_));
	#endif

 	switch(mh->dh_fc.fc_subtype) {
	case MAC_Subtype_RTS:
		if(! is_idle()) {
		#ifdef DEBUG
		fprintf(stdout, "%f \tNode:%d in check_pktRTS it's  not idle, setting backoff timer\n", Scheduler::instance().clock(), index_);
		#endif
			inc_cw();
			mhBackoff_.start(cw_, is_idle());
			return 0;
		}
//duy
	
		switch(ammac_cur_state_) {

		case AMMAC_IDLE:
			if( (ammacBuildFreeChannelList() <= 0) ||
				ammacIsRcvrBusy(dst))
			{
				#ifdef DEBUG
				fprintf(stdout, "%f \tNode:%d in check_pktRTS,recv busy or no free channels\n", Scheduler::instance().clock(), index_);
				#endif
				commonChan = ammacUsageOnCommonChannel();
				if(commonChan == 0.0) {
					inc_cw();
					mhBackoff_.start(cw_, is_idle());
				}
				else {

				//	set_nav(usec(commonChan)); //add CS
				//use wait is ok

					if(mhWait_.busy())
						mhWait_.stop();
					mhWait_.start(commonChan);
				}
				return 0;
			}

	#ifdef DEBUG

	for(int i= NUM_OF_CHANNELS-1; i>=0; i--)
		fprintf(stdout, "***channel time usage %f by the pair %d and %d on channel %d\n", ammac_cul_[i], ammac_cul_node_[i][0],ammac_cul_node_[i][1], i);

	#endif
			
			//check usage on common channel here one more time
			commonChan = ammacUsageOnCommonChannel();
			if(commonChan == 0.0) {
				;
			}
			else {
				//set_nav(usec(commonChan)); //add CS
				if(mhWait_.busy())
					mhWait_.stop();
				mhWait_.start(commonChan);
				return 0;
			}
			
			

			//check if other channels are taken
			chan_stats = ammacUsageChanStats();

			fprintf(stdout, "chanstats %f\n", chan_stats);

			ammac_cur_state_ = AMMAC_CTRL_WIN;

			if(chan_stats == 0.0) {
			

			//schedule initiator
			rf->rf_initiator = 1;

			// time it takes for a number of successful negotiation
			free_time = one_nego*(NUM_OF_CHANNELS - 1);
			
			my_data = txtime(pktTx_) + delay;
			temp_data = my_data ;

			my_data = my_data + free_time;

			data_limit = ll_->ifq()->get_win_time(my_data + data_limit, dst, delay);

			if(data_limit <= 0)
				;
			else
				my_data = data_limit;

		/*
			while (my_data <= (free_time + added_time)){
				if(my_data + temp_data <= (free_time + added_time))
					my_data += temp_data;
				else 
				 	break;
			}
			*/
		//	if(temp_data < data_limit)
		//		my_data += temp_data;
			
				

			rf->rf_duration_data_win = usec_32(my_data);
			set_data_transfer(my_data);

			total_time = one_nego * NUM_OF_CHANNELS;

			#ifdef DEBUG
			fprintf(stdout, "%f \tNode:%d in check_pktRTS,time for 1 neg is %f \n", Scheduler::instance().clock(), index_,one_nego);
			fprintf(stdout, "%f \tNode:%d in check_pktRTS,time for 3 neg is %f \n", Scheduler::instance().clock(), index_,total_time);
			#endif

			//finally  this is the ctrl_time
			ammac_ctrl_time_ = total_time + NOW + padding;

			if(mhCtrl_.busy()) 
				mhCtrl_.stop();

			mhCtrl_.start( total_time+padding);

			//convert it and assign to ctrl_win
			rf->rf_duration_ctrl_win = usec_32(total_time+padding);


			} //end if from chan_stats
			else {

				rf->rf_initiator = 0;

				//cling to  others
				my_data = txtime(pktTx_) + delay;

				if(my_data > (chan_stats - one_nego))
					return 0;
				my_data = chan_stats - one_nego;

				data_limit = ll_->ifq()->get_win_time(my_data, dst, delay);

				if(data_limit <= 0)
					;
				else
					my_data = data_limit;

				rf->rf_duration_ctrl_win = usec_32(one_nego + padding);

				rf->rf_duration_data_win = usec_32(my_data);

				set_data_transfer(my_data);

				ammac_ctrl_time_ = one_nego + NOW +  padding;
                                                                      
				if(mhCtrl_.busy())
					mhCtrl_.stop();
				mhCtrl_.start(one_nego+ padding);
			}

			
			#ifdef DEBUG
			fprintf(stdout, "%f \tNode:%d in check_pktRTS My ammac_ctrl_time_ is %f\n", Scheduler::instance().clock(), index_, mhCtrl_.expire()+NOW);
			fprintf(stdout, "%f \tNode:%d in check_pktRTS My data_time is %f\n", Scheduler::instance().clock(), index_, my_data+NOW);
			#endif

			//get fcl_ from ammacBuildFreeChannelList from the check point above
			//update rf_channel_list
			for (int i=0; i< NUM_OF_CHANNELS; i++) {
				rf->rf_channel_list[i] = fcl_[i];
			}
			break;


		case AMMAC_CTRL_WIN:

			rf->rf_initiator = 0;

		//	set_data_transfer(sec_uint(rf->rf_duration_data_win));
			
			if( (ammacBuildFreeChannelList()) <= 0 || ( ammacIsRcvrBusy(dst))) {

				#ifdef DEBUG
				printf("In check_pktRTS during ctrl_win, I am starving \n");
				#endif

				commonChan = ammacUsageOnCommonChannel();
				if(commonChan == 0.0) {
			//		inc_cw();
			//		mhBackoff_.start(cw_, is_idle());
				}
				else {
					//set_nav(usec(commonChan)); //add CS
					if(mhWait_.busy())
						mhWait_.stop();
					mhWait_.start(commonChan);
				}
				return 0;
			}
				

//			time_left = ammac_ctrl_time_ - NOW;


			//this is the key
			time_left = mhCtrl_.expire();




			//means that you won't be able to make it in the remaining time for a
			//succesfully negotiation, so just back off, to the ammac_data_time
			//that's when everyone comes back
			if(time_left < one_nego) {
				#ifdef DEBUG
				fprintf(stdout, "Not enought time for one more negotiations, must back off time left is %f total time is %f\n", time_left, one_nego);
				#endif
				ammacClearVars();
				commonChan = ammacUsageOnCommonChannel();
				fprintf(stdout, "commont chan %f\n", commonChan);
				if(commonChan == 0.0) {
					
					//inc_cw();
					//mhBackoff_.start(cw_, is_idle());
				}
				else {
				//	set_nav(usec(commonChan)); //add CS
					if(mhWait_.busy())
						mhWait_.stop();
					mhWait_.start(commonChan);
				}
				return 0;
			}
			else {
				#ifdef DEBUG
				fprintf(stdout, "%f \tNode:%d, in check_pktRTS enough time left for %f negotiation(s)\n", Scheduler::instance().clock(), index_, (time_left/one_nego));
				#endif
				for(int i=0; i < NUM_OF_CHANNELS; i++) 
					rf-> rf_channel_list[i] = fcl_[i];

				//this is the ticking clock
//				rf->rf_duration_ctrl_win = usec_32(ammac_ctrl_time_ - NOW);


				rf->rf_duration_ctrl_win = usec_32(mhCtrl_.expire());

				ammac_ctrl_time_  = mhCtrl_.expire() + NOW;

				my_data = txtime(pktTx_) + delay;
				temp_data = my_data;
				remain = time_left - one_nego;
				if(remain > 0) {
					my_data = my_data + remain - padding;
					data_limit = ll_->ifq()->get_win_time(my_data + data_limit, dst, delay);

					if(data_limit <= 0)
						;
					else
						my_data = data_limit;

					/*
					while(my_data <= (remain + added_time)) {
						if(my_data + temp_data <= (remain + added_time))
							my_data += temp_data;
						else 	
							break;
					}
					if((remain - total_time) >= 0)
						if(temp_data < data_limit)
							my_data += temp_data;
		//					*/

				}

				rf->rf_duration_data_win = usec_32(my_data);
				set_data_transfer(my_data);

			#ifdef DEBUG
			fprintf(stdout, "%f \tNode:%d in check_pktRTS My ammac_ctrl_time_ is %f\n", Scheduler::instance().clock(), index_, mhCtrl_.expire()+NOW);
			fprintf(stdout, "%f \tNode:%d in check_pktRTS get_data_transfer is %f\n", Scheduler::instance().clock(), index_,get_data_transfer());
			fprintf(stdout, "%f \tNode:%d in check_pktRTS My data_time is %f\n", Scheduler::instance().clock(), index_, txtime(pktTx_) + delay);
			#endif
			}

		break;

		case AMMAC_DATA_WIN:

			//set_data_transfer( sec_uint(rf->rf_duration_data_win));

			#ifdef DEBUG
			printf("you are transmitting, must back off\n");
			#endif
			if( mhRdezvous_.busy() == 1) {
				if(mhWait_.busy())
					mhWait_.stop();
				mhWait_.start(mhRdezvous_.expire());
			}
			return 0;
			break;
		default:
			printf("ERROR: non-ammac state in check pkt RTS\n");
			exit(1);
		}
//end of duy

		setTxState(MAC_RTS);
		timeout = txtime(phymib_.getRTSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay                      // XXX
			+ phymib_.getSIFS()
			+ txtime(phymib_.getCTSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay;
		break;
	default:
		fprintf(stderr, "check_pktRTS:Invalid MAC Control subtype\n");
		exit(1);
	}

	transmit(pktRTS_, timeout);
	return 0;
}

//duy

double
Mac802_11::ammacFollowInitiator()
{
//	double data_limit =  .002630;
//	double data_limit =  .002000;
//	double data_limit =  .000694;
	double data_limit =  .000424;
	double my_data = 0;
	double one_nego = 
			+ txtime(phymib_.getCTSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
			+ txtime(phymib_.getATSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay + phymib_.getSIFS();

	double remain = ammac_cul_[NUM_OF_CHANNELS-1] - NOW;

	if((remain - one_nego) >=  data_limit) {

		/*
		while (my_data <= remain){
				if(my_data + data_limit <= remain)
					my_data += data_limit;
				else 
				 	break;
			}
		return my_data;
		*/

		return (remain - one_nego);
		
	}
	return 0.0;
}

int
Mac802_11::ammacBuildFreeChannelList()
{
	int stat =0;
	fcl_[0] =0;
	for(int i=0; i<NUM_OF_CHANNELS; i++) {

		//ammac_cul = current usage list
		if(ammac_cul_[i] <= NOW) {
			fcl_[i] = 1;
			stat++;
		}
		else	
			fcl_[i] = 0;
	}
	return stat;
}	
double 
Mac802_11::ammacUsageChanStats(void)
{
	for(int i = NUM_OF_CHANNELS - 1; i > 0; i--) {
		if(ammac_cul_[i] > NOW)
			return(ammac_cul_[i] - NOW);
	}
	return 0.0;

}

double
Mac802_11::ammacUsageOnCommonChannel()
{
	if(ammac_cul_[0] > NOW ) 
		return (ammac_cul_[0] - NOW);
	return 0.0;
}

int
Mac802_11::ammacIsRcvrBusy(int rcvr) 
{
	for(int i=0; i<NUM_OF_CHANNELS; i++)
	{
		if((ammac_cul_node_[i][0] == rcvr) && (ammac_cul_[i] > NOW)) {
			#ifdef DEBUG
			fprintf(stdout, "recever wont be free until %f", ammac_cul_[i]);
			#endif
			return 1;
			}
		else if((ammac_cul_node_[i][1] == rcvr) && (ammac_cul_[i] > NOW)) {
			#ifdef DEBUG
			fprintf(stdout, "recever wont be free until %f", ammac_cul_[i]);
			#endif
			return 1;
			}
	}			
	return 0;
}

int
Mac802_11::ammacSelectChannel()
{
	ammacBuildFreeChannelList();
	for(int i= NUM_OF_CHANNELS-1; i>=0; i--)
	{
		if( (fcl_[i] == 1) && (ammac_recved_fcl_[i] == 1))
			return i;
	}
	return -1;
}

void
Mac802_11::ammacUpdateChansInfo()
{
	//todo add check one nego for RTS  CTS and ATS before assigning it

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in ammacUpdateChansInfo\n", Scheduler::instance().clock(), index_);
	#endif
	hdr_mac802_11 * mh = HDR_MAC802_11(pktRx_);
	u_int32_t src = ETHER_ADDR(mh->dh_ta);
	u_int32_t dst = ETHER_ADDR(mh->dh_ra);

	struct rts_frame *rf;
	struct cts_frame *cf;
	struct ats_frame *ats;

	double new_ctrl_time, data_time;
	double one_nego = txtime(phymib_.getRTSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
			+ txtime(phymib_.getCTSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
			+ txtime(phymib_.getATSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay;


	u_int8_t type = mh->dh_fc.fc_type;
	u_int8_t subtype = mh->dh_fc.fc_subtype;

	switch(type) {

	case MAC_Type_Management:
		discard(pktRx_, DROP_MAC_PACKET_ERROR);
		break;
	case MAC_Type_Control:
		switch(subtype) {

		case MAC_Subtype_RTS:

		rf = (struct rts_frame*) pktRx_->access(hdr_mac::offset_);

		#ifdef DEBUG
		fprintf(stdout, "%f \tNode:%d in ammacUpdateChansInfo RTS\n", Scheduler::instance().clock(), index_);
		#endif

		switch(ammac_cur_state_){
		case AMMAC_IDLE:
		
		/*
			//only grab the time
			if(rf->rf_initiator = 0){
				new_ctrl_time = sec_uint(rf->rf_duration_ctrl_win) + NOW - txtime(phymib_.getRTSlen(), basicRate_) - DSSS_MaxPropagationDelay;
				
				if((new_ctrl_time - one_nego) < NOW) 
					;
				else {
					ammac_ctrl_time_ = new_ctrl_time;
					if(mhCtrl_.busy()) 
						mhCtrl_.stop();
					mhCtrl_.start(ammac_ctrl_time_ - NOW);
					ammac_cur_state_ = AMMAC_CTRL_WIN;
				}*/
				/*
				if(( mhCtrl_.expire() - one_nego) <  0) {
					if((new_ctrl_time - one_nego) >= 0.0) {
						ammac_ctrl_time_ = new_ctrl_time + NOW;
						if(mhCtrl_.busy())
							mhCtrl_.stop();
						mhCtrl_.start(new_ctrl_time);
					} else 
						return;

				} 
				else ; //do nothing
				*/
			//}
		    	break;

		case AMMAC_CTRL_WIN: 

			/*
			//only grab the time
			if(rf->rf_initiator = 0){
				new_ctrl_time = sec_uint(rf->rf_duration_ctrl_win) + NOW - txtime(phymib_.getRTSlen(), basicRate_) - DSSS_MaxPropagationDelay;
				
				if((new_ctrl_time - one_nego) < NOW) 
					;
				else {
					ammac_ctrl_time_ = new_ctrl_time;
					if(mhCtrl_.busy()) 
						mhCtrl_.stop();
					mhCtrl_.start(ammac_ctrl_time_ - NOW);
				}*/
				/*
				if(( mhCtrl_.expire() - one_nego) <  0) {
					if((new_ctrl_time - one_nego) >= 0.0) {
						ammac_ctrl_time_ = new_ctrl_time + NOW;
						if(mhCtrl_.busy())
							mhCtrl_.stop();
						mhCtrl_.start(new_ctrl_time);
					} else 
						return;

				} 
				else ; //do nothing
				*/
//			}
			break;
		case AMMAC_DATA_WIN:
			//it means you're transmitting forget it
			/*
			discard(pktRx_, DROP_MAC_BUSY);
			#ifdef DEBUG
			printf("SOS in ammacUpdateChansInfo datawin RTS\n");
			#endif
			if(mhRdezvous_.busy() == 1) {
				if(mhWait_.busy()) 
					mhWait_.stop();
				mhWait_.start(mhRdezvous_.expire());
			}*/
			break;
		default:
			break;
		}
		break;


	case MAC_Subtype_CTS:
		cf = (struct cts_frame*) pktRx_->access(hdr_mac::offset_);

		#ifdef DEBUG
		fprintf(stdout, "%f \tNode:%d in ammacUpdateChansInfo CTS\n selected channel is %d\n", Scheduler::instance().clock(), index_, cf->cf_selected_channel);
		#endif

		switch(ammac_cur_state_) {
		case AMMAC_IDLE:

			new_ctrl_time = sec_uint(cf->cf_duration_ctrl_win) + NOW - txtime(phymib_.getCTSlen(), basicRate_) - DSSS_MaxPropagationDelay;
			data_time =  phymib_.getSIFS()
				+ txtime(phymib_.getATSlen(), basicRate_) + DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ sec_uint(cf->cf_duration_data_win );
/*
			if(( mhCtrl_.expire() - one_nego) <  0) {
				if((new_ctrl_time - one_nego) >= 0.0) {
					ammac_ctrl_time_ = new_ctrl_time + NOW;
					if(mhCtrl_.busy())
						mhCtrl_.stop();
					mhCtrl_.start(new_ctrl_time);
				} else 
					return;

			} 
			else ; //do nothing
			*/
			if((new_ctrl_time - one_nego) <= NOW)  {
				printf("love\n");
			}
			else {
				ammac_ctrl_time_ = new_ctrl_time;
				if(mhCtrl_.busy()) 
					mhCtrl_.stop();
				mhCtrl_.start(ammac_ctrl_time_ - NOW);
				ammac_cur_state_ = AMMAC_CTRL_WIN;
			}

//			ammac_total_pkts_in_next_slot_ = 0;
//			ammac_total_nodes_in_next_slot_ = 0;

			//how many pkts request in cts pkt
//			if(cf->cf_pkts_for_next_slot > 0) {
//				ammac_total_pkts_in_next_slot_ += cf->cf_pkts_for_next_slot;
//				ammac_total_nodes_in_next_slot_++;
//			}

			if(cf->cf_selected_channel >= 0) 
				ammacUpdateChannelUsageList(data_time + NOW, cf->cf_selected_channel, (int)src, (int)dst);
			break;	
		case AMMAC_CTRL_WIN:

			new_ctrl_time = sec_uint(cf->cf_duration_ctrl_win) + NOW - txtime(phymib_.getCTSlen(), basicRate_) - DSSS_MaxPropagationDelay;
			data_time =  phymib_.getSIFS()
				+ txtime(phymib_.getATSlen(), basicRate_) + DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ sec_uint(cf->cf_duration_data_win );
			//	+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
			//	+ txtime(phymib_.getACKlen(), basicRate_)
			//	+ DSSS_MaxPropagationDelay;
			/*
			if(( mhCtrl_.expire() - one_nego) <  0) {
				if((new_ctrl_time - one_nego) >= 0.0) {
					ammac_ctrl_time_ = new_ctrl_time + NOW;
					if(mhCtrl_.busy())
						mhCtrl_.stop();
					mhCtrl_.start(new_ctrl_time);
				} else 
					return;

			} 
			else ; //do nothing
			*/

			if((new_ctrl_time - one_nego) <= NOW) 
				printf("love\n");
			else {
				ammac_ctrl_time_ = new_ctrl_time;
				if(mhCtrl_.busy()) 
					mhCtrl_.stop();
				mhCtrl_.start(ammac_ctrl_time_ - NOW);
			}

//			if(cf->cf_pkts_for_next_slot > 0) {
//				ammac_total_pkts_in_next_slot_ += cf->cf_pkts_for_next_slot;
//				ammac_total_nodes_in_next_slot_++;
//			}

			if(cf->cf_selected_channel >= 0) 
				ammacUpdateChannelUsageList(data_time + NOW, cf->cf_selected_channel, (int)src, (int)dst);
			break;

		case AMMAC_DATA_WIN:


			discard(pktRx_, DROP_MAC_BUSY);

/*
			//for transceiver part, save ctrl time too
			new_ctrl_time = sec_uint(cf->cf_duration_ctrl_win) + NOW - txtime(phymib_.getCTSlen(), basicRate_) - DSSS_MaxPropagationDelay;
			data_time =  phymib_.getSIFS()
				+ txtime(phymib_.getATSlen(), basicRate_) + DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ sec_uint(cf->cf_duration_data_win )
				+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getACKlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;
			if(cf->cf_selected_channel >= 0) 
				ammacUpdateChannelUsageList(data_time+NOW, cf->cf_selected_channel,(int)src, (int)dst);

			//ammacUpdateMyTimers(new_ctrl_time,data_time);
//			if(mhRdezvous_.busy() == 1) {
//				if(mhWait_.busy()) 
//					mhWait_.stop();
//				mhWait_.start(mhRdezvous_.expire());
//			}*/
			break;

		default:
			printf("%f	Node:%d	ERROR: getting CTS in no state\n", NOW, index_);
			exit(1);
		}
		break;
	case MAC_Subtype_ATS:
		ats = (struct ats_frame*)pktRx_->access(hdr_mac::offset_);

		#ifdef DEBUG
		fprintf(stdout, "%f \tNode:%d in ammacUpdateChansInfo ATS selected channel %d\n", Scheduler::instance().clock(), index_, ats->ats_confirmed_channel);
		#endif
		if(ats->ats_confirmed_channel < 0) 
			break;
		switch(ammac_cur_state_) {
		case AMMAC_IDLE:

			new_ctrl_time = sec_uint(ats->ats_duration_ctrl_win) + NOW - txtime(phymib_.getATSlen(), basicRate_) - DSSS_MaxPropagationDelay;
			data_time = phymib_.getSIFS()
				+ sec_uint(ats->ats_duration_data_win );
			//	+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
			//	+ txtime(phymib_.getACKlen(), basicRate_)
			//	+ DSSS_MaxPropagationDelay;

			/*
			if(( mhCtrl_.expire() - one_nego) <  0) {
				if((new_ctrl_time - one_nego) >= 0.0) {
					ammac_ctrl_time_ = new_ctrl_time + NOW;
					if(mhCtrl_.busy())
						mhCtrl_.stop();
					mhCtrl_.start(new_ctrl_time);
				} else 
					return;

			} 
			else ; //do nothing
			*/
			if((new_ctrl_time - one_nego) <= NOW) 
				printf("love\n");
			else {
				ammac_ctrl_time_ = new_ctrl_time;
				if(mhCtrl_.busy()) 
					mhCtrl_.stop();
				mhCtrl_.start(ammac_ctrl_time_ - NOW);
				ammac_cur_state_ = AMMAC_CTRL_WIN;
			}

//			ammac_total_pkts_in_next_slot_ = 0;
//			ammac_total_nodes_in_next_slot_ = 0;

//			if(ats->ats_pkts_for_next_slot > 0) {
////				ammac_total_pkts_in_next_slot_ += ats->ats_pkts_for_next_slot;
////				ammac_total_nodes_in_next_slot_++;
////			}

			if(ats->ats_confirmed_channel >= 0)
				ammacUpdateChannelUsageList(data_time + NOW, ats->ats_confirmed_channel, (int)src, (int)dst);
			break;
		case AMMAC_CTRL_WIN:
////			if(ats->ats_pkts_for_next_slot > 0) {
//				ammac_total_pkts_in_next_slot_ += ats->ats_pkts_for_next_slot;
//				ammac_total_nodes_in_next_slot_++;
//			}

			new_ctrl_time = sec_uint(ats->ats_duration_ctrl_win) + NOW - txtime(phymib_.getATSlen(), basicRate_) - DSSS_MaxPropagationDelay;
			data_time =  phymib_.getSIFS()
				+ sec_uint(ats->ats_duration_data_win );
		//		+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
		//		+ txtime(phymib_.getACKlen(), basicRate_)
		//		+ DSSS_MaxPropagationDelay;

/*
			if(( mhCtrl_.expire() - one_nego) <  0) {
				if((new_ctrl_time - one_nego) >= 0.0) {
					ammac_ctrl_time_ = new_ctrl_time + NOW;
					if(mhCtrl_.busy())
						mhCtrl_.stop();
					mhCtrl_.start(new_ctrl_time);
				} else 
					return;

			} 
			else ; //do nothing
			*/
			if((new_ctrl_time - one_nego) <= NOW) 
				printf("love\n");
			else {
				ammac_ctrl_time_ = new_ctrl_time;
				if(mhCtrl_.busy()) 
					mhCtrl_.stop();
				mhCtrl_.start(ammac_ctrl_time_ - NOW);
			}

			if(ats->ats_confirmed_channel >= 0)
				ammacUpdateChannelUsageList(data_time + NOW, ats->ats_confirmed_channel, (int)src,(int) dst);
			break;

		case AMMAC_DATA_WIN:
		
			discard(pktRx_, DROP_MAC_BUSY);
/*
			//for transceiver part, save ctrl time too
			new_ctrl_time = sec_uint(ats->ats_duration_ctrl_win) + NOW - txtime(phymib_.getATSlen(), basicRate_) - DSSS_MaxPropagationDelay;
			data_time =  phymib_.getSIFS()
				+ sec_uint(ats->ats_duration_data_win )
				+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getACKlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;
					

			if(ats->ats_confirmed_channel >= 0)
				ammacUpdateChannelUsageList(data_time + NOW, ats->ats_confirmed_channel, (int)src, (int)dst);
//			if( mhRdezvous_.busy() == 1) {
//				if(mhWait_.busy()) 
//					mhWait_.stop();
//				mhWait_.start(mhRdezvous_.expire());
//			}*/
			break;
		default:
			printf("%f	%d	ERROR bad state %d in updateammacwindows\n", NOW, index_,ammac_cur_state_);
			exit(1);
		}
		break;

		case MAC_Subtype_ACK:
		break;

		default:
		fprintf(stderr, "error %x\n", subtype);
		exit(1);
		}

		break;
	case MAC_Type_Data:
		switch(subtype) {
		case MAC_Subtype_Data:
			break;
		default:
			fprintf(stderr, "updateMacapRdezvous invalid control subtye  error %x\n", subtype);
			exit(1);
		}
		break;
	default:
		fprintf(stderr, "error %x\n", subtype);
		exit(1);
	}
	#ifdef DEBUG
	for(int i= NUM_OF_CHANNELS-1; i>=0; i--)
		fprintf(stdout, "***channel time usage %f by the pair %d and %d on channel %d\n", ammac_cul_[i], ammac_cul_node_[i][0],ammac_cul_node_[i][1], i);
	#endif
}


void
Mac802_11::ammacUpdateChannelUsageList(double t, int channel, int src, int dst)
{
		
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in ammacUpdateChannelUsageList time is %f chan is %d \n", Scheduler::instance().clock(), index_, t, channel);
	#endif
	if( t > ammac_cul_[channel]){
		ammac_cul_[channel] = t;
		ammac_cul_node_[channel][0] = src;
		ammac_cul_node_[channel][1] = dst;
	}
	return;

}


void
Mac802_11::ammacUpdateMyTimers(double c, double d)
{
	ammac_ctrl_time_ = c + NOW;

	if(mhCtrl_.busy()) 
		mhCtrl_.stop();
	mhCtrl_.start(c);

	ammac_data_time_  = d;
}
					

		
void
Mac802_11::ammacClearChannelState() {

	setRxState(MAC_IDLE);
	if(mhRecv_.busy())
		mhRecv_.stop();
	if(pktRx_) {
		Packet::free(pktRx_);
		pktRx_ = 0;
	}
}


//end of duy			

	
	



//ddn
//checking pktTx_ holding a pkt, return 0 if it holds, otherwise -1
//if channel is idle, pkt would be sent by calling transmit set transmission state
//to MAC_SEND
//Otherwise, backoff timer would be set(increasing contention window as well)

int
Mac802_11::check_pktTx()
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in check_pktTx\n", Scheduler::instance().clock(), index_);
	#endif

	struct hdr_mac802_11 *mh;
	double timeout;
	
	assert(mhBackoff_.busy() == 0);

	if(pktTx_ == 0)
		return -1;

	mh = HDR_MAC802_11(pktTx_);

//duy
	if(ammac_cur_state_ != AMMAC_DATA_WIN) {

		#ifdef DEBUG
		printf(" i want to transmit but my state is not AMMAC_DATA_WIN\n");
		#endif

		sendRTS((int)ETHER_ADDR(mh->dh_ra));
		inc_cw();
		mhBackoff_.start(cw_, is_idle());
		return 0;
	}
	else {

		#ifdef DEBUG
		fprintf(stdout, "my data transfer time is %f\n", get_data_transfer());
		#endif
	}
//end of duy	
				
	switch(mh->dh_fc.fc_subtype) {
	case MAC_Subtype_Data:
	
		//added here should do it www
		if(! is_idle() && (mh->dh_fc.fc_channel == 0)) {
			sendRTS(ETHER_ADDR(mh->dh_ra));
			inc_cw();
			mhBackoff_.start(cw_, is_idle());
			return 0;
		}

		setTxState(MAC_SEND);
		if((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST)
                        timeout = txtime(pktTx_)
                                + DSSS_MaxPropagationDelay              // XXX
                               + phymib_.getSIFS()
                               + txtime(phymib_.getACKlen(), basicRate_)
                               + DSSS_MaxPropagationDelay;             // XXX
		else
			timeout = txtime(pktTx_);
		break;
	default:
		fprintf(stderr, "check_pktTx:Invalid MAC Control subtype\n");
		exit(1);
	}
	if(active_channel_ != -1 ) {
		mh->dh_fc.fc_channel = selected_channel_;
		//mh->dh_fc.fc_channel = active_channel_;
	} else {
		mh->dh_fc.fc_channel =  DEFAULT_CHANNEL;
	}

	transmit(pktTx_, timeout);
	return 0;
}
/*
 * Low-level transmit functions that actually place the packet onto
 * the channel.
 */
void
Mac802_11::sendRTS(int dst)
{
	double delay = DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getACKlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In sendRTS, would like to send to %d\n", Scheduler::instance().clock(), index_, dst);
	fprintf(stdout, "%f \tNode:%d In sendRTS at ammac state %d (0=idle, 1=ctrl, 2=data) active channel is %d ammac_ctrl_time is %f\n", Scheduler::instance().clock(), index_, ammac_cur_state_, active_channel_, ammac_ctrl_time_);
	fprintf(stdout, "%f \tNode:%d In sendRTS, data would take  %f\n", Scheduler::instance().clock(), index_, txtime(pktTx_));
	fprintf(stdout, "%f \tNode:%d In sendRTS, delay takes  %f\n", Scheduler::instance().clock(), index_, delay);
	#endif

	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	struct rts_frame *rf = (struct rts_frame*)p->access(hdr_mac::offset_);
//	double data_time;
	
	assert(pktTx_);
	assert(pktRTS_ == 0);

	/*
	 *  If the size of the packet is larger than the
	 *  RTSThreshold, then perform the RTS/CTS exchange.
	 */
	if( (u_int32_t) HDR_CMN(pktTx_)->size() < macmib_.getRTSThreshold() ||
            (u_int32_t) dst == MAC_BROADCAST) {
		Packet::free(p);
		return;
	}

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getRTSlen();
	ch->iface() = -2;
	ch->error() = 0;

	bzero(rf, MAC_HDR_LEN);

	rf->rf_fc.fc_protocol_version = MAC_ProtocolVersion;
 	rf->rf_fc.fc_type	= MAC_Type_Control;
 	rf->rf_fc.fc_subtype	= MAC_Subtype_RTS;
 	rf->rf_fc.fc_to_ds	= 0;
 	rf->rf_fc.fc_from_ds	= 0;
 	rf->rf_fc.fc_more_frag	= 0;
 	rf->rf_fc.fc_retry	= 0;
 	rf->rf_fc.fc_pwr_mgt	= 0;
 	rf->rf_fc.fc_more_data	= 0;
 	rf->rf_fc.fc_wep	= 0;
 	rf->rf_fc.fc_order	= 0;

//duy
	rf->rf_initiator = 0;		
	rf->rf_duration_ctrl_win = 0;	

//this is for testing
//	double data_limit = ll_->ifq()->get_win_time(.010000, dst, delay);

//	data_time = txtime(pktTx_) + delay;
//	rf->rf_duration_data_win = usec_32(data_time);
//	set_data_transfer(data_time);	

	//initialization, to be set in check_pktRTS
	for(int i=0; i< NUM_OF_CHANNELS; i++)
		rf->rf_channel_list[i] = -1;

	if(active_channel_ != DEFAULT_CHANNEL) {
		;
	}

	//should start with the default channel
	rf->rf_fc.fc_channel = DEFAULT_CHANNEL;

//end of duy
	
	//rf->rf_duration = RTS_DURATION(pktTx_);
	STORE4BYTE(&dst, (rf->rf_ra));
	
	/* store rts tx time */
 	ch->txtime() = txtime(ch->size(), basicRate_);
	
	STORE4BYTE(&index_, (rf->rf_ta));


//duy

	/* calculate rts duration field */	
	rf->rf_duration = usec(phymib_.getSIFS()
			       + txtime(phymib_.getCTSlen(), basicRate_)
			       + phymib_.getSIFS()
			       + txtime(phymib_.getATSlen(), basicRate_));

//end of duy
	//delegate to pktRTS
	pktRTS_ = p;
}

//duy
void
Mac802_11::sendATS(int dst) 
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In sendATS\n", Scheduler::instance().clock(), index_);
	#endif


	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);

	struct ats_frame *rf = (struct ats_frame *)p->access(hdr_mac::offset_);

	assert(pktTx_);
	assert(pktATS_ == 0);

	ch->uid() =0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getATSlen();
	ch->iface() = -2;
	ch->error() = 0;

	bzero(rf, MAC_HDR_LEN);

	rf->ats_fc.fc_protocol_version = MAC_ProtocolVersion;
	rf->ats_fc.fc_type = MAC_Type_Control;
	rf->ats_fc.fc_subtype = MAC_Subtype_ATS;
	rf->ats_fc.fc_to_ds = 0;
	rf->ats_fc.fc_from_ds = 0;
	rf->ats_fc.fc_more_frag = 0;
	rf->ats_fc.fc_retry = 0;
	rf->ats_fc.fc_pwr_mgt = 0;
	rf->ats_fc.fc_more_data = 0;
	rf->ats_fc.fc_wep = 0;
	rf->ats_fc.fc_order = 0;

	rf->ats_duration_ctrl_win = 0;
	rf->ats_duration_data_win = usec_32(get_data_transfer());
	rf->ats_confirmed_channel = selected_channel_;

	if(active_channel_ != DEFAULT_CHANNEL) {
		;	
	}
	rf->ats_fc.fc_channel = DEFAULT_CHANNEL;

//	u_int32_t d = ll_->ifq()->getNextDest();
//	if (((int)d < MAX_NODE) && ((int)d > -1)) {
//		rf->ats_pkts_for_next_slot = ll_->ifq()->countDestPkts(d);
//	}
//	else
//		rf->ats_pkts_for_next_slot = 0;

	STORE4BYTE(&dst, (rf->ats_ra));
	STORE4BYTE(&index_, (rf->ats_ta));
	
	ch->txtime() = txtime(ch->size(), basicRate_);

	rf->ats_duration = 0;

	pktATS_ = p;
}


//end of duy

void
Mac802_11::sendCTS(int dst, double rts_duration)
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In sendCTS\n", Scheduler::instance().clock(), index_);
	fprintf(stdout, "%f \tNode:%d sendCTS active_channel is %d\n", Scheduler::instance().clock(), index_, active_channel_);
	#endif

	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	struct cts_frame *cf = (struct cts_frame*)p->access(hdr_mac::offset_);
	u_int32_t d =0;

	assert(pktCTRL_ == 0);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getCTSlen();


	ch->iface() = -2;
	ch->error() = 0;
	//ch->direction() = hdr_cmn::DOWN;
	bzero(cf, MAC_HDR_LEN);

	cf->cf_fc.fc_protocol_version = MAC_ProtocolVersion;
	cf->cf_fc.fc_type	= MAC_Type_Control;
	cf->cf_fc.fc_subtype	= MAC_Subtype_CTS;
 	cf->cf_fc.fc_to_ds	= 0;
 	cf->cf_fc.fc_from_ds	= 0;
 	cf->cf_fc.fc_more_frag	= 0;
 	cf->cf_fc.fc_retry	= 0;
 	cf->cf_fc.fc_pwr_mgt	= 0;
 	cf->cf_fc.fc_more_data	= 0;
 	cf->cf_fc.fc_wep	= 0;
 	cf->cf_fc.fc_order	= 0;

	cf->cf_fc.fc_channel = DEFAULT_CHANNEL;

	cf->cf_duration_data_win = usec_32(get_data_transfer()); 


	//wrt ritesh
//	if(pktTx_) {
//		struct hdr_mac802_11* dh = HDR_MAC802_11(pktTx_);
//		d = ETHER_ADDR(dh->dh_ra);
//	}
//	else
//		d = ll_->ifq()->getNextDest();
//	if( ((int) d < MAX_NODE) && ((int)d > -1)) {
//		cf->cf_pkts_for_next_slot = ll_->ifq()->countDestPkts(d) + 1;
//	}
//	else
//		cf->cf_pkts_for_next_slot = 0;


	
	//cf->cf_duration = CTS_DURATION(rts_duration);
	STORE4BYTE(&dst, (cf->cf_ra));

	STORE4BYTE(&index_, (cf->cf_ta)); //duy
	
	/* store cts tx time */
	ch->txtime() = txtime(ch->size(), basicRate_);
	
//duy	
	/* calculate cts duration */
  	cf->cf_duration = usec(phymib_.getSIFS() 
				+ txtime(phymib_.getATSlen(), basicRate_));
	
//end of duy

	//delegate to pktCTRL_
	pktCTRL_ = p;
	
}

void
Mac802_11::sendACK(int dst)
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In sendACK\n", Scheduler::instance().clock(), index_);
	#endif
	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	struct ack_frame *af = (struct ack_frame*)p->access(hdr_mac::offset_);

	assert(pktCTRL_ == 0);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	// CHANGE WRT Mike's code
	ch->size() = phymib_.getACKlen();
	ch->iface() = -2;
	ch->error() = 0;
	
	bzero(af, MAC_HDR_LEN);

	af->af_fc.fc_protocol_version = MAC_ProtocolVersion;
 	af->af_fc.fc_type	= MAC_Type_Control;
 	af->af_fc.fc_subtype	= MAC_Subtype_ACK;
 	af->af_fc.fc_to_ds	= 0;
 	af->af_fc.fc_from_ds	= 0;
 	af->af_fc.fc_more_frag	= 0;
 	af->af_fc.fc_retry	= 0;
 	af->af_fc.fc_pwr_mgt	= 0;
 	af->af_fc.fc_more_data	= 0;
 	af->af_fc.fc_wep	= 0;
 	af->af_fc.fc_order	= 0;

//duy
	if(active_channel_ != -1) {
		af->af_fc.fc_channel = active_channel_;
	}else  {
		af->af_fc.fc_channel = DEFAULT_CHANNEL; 
	}
//end of duy

	//af->af_duration = ACK_DURATION();
	STORE4BYTE(&dst, (af->af_ra));

	/* store ack tx time */
 	ch->txtime() = txtime(ch->size(), basicRate_);
	
	/* calculate ack duration */
 	af->af_duration = 0;	


	//delegate to pktCTRL_
	pktCTRL_ = p;
}

void
Mac802_11::sendDATA(Packet *p)
{	



	hdr_cmn* ch = HDR_CMN(p);
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	int dst = (int) ETHER_ADDR(dh->dh_ra);

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In sendDATA, would like to send to %d\n", Scheduler::instance().clock(), index_,dst);
	#endif

	assert(pktTx_ == 0);

	/*
	 * Update the MAC header
	 */
	ch->size() += phymib_.getHdrLen11();

	dh->dh_fc.fc_protocol_version = MAC_ProtocolVersion;
	dh->dh_fc.fc_type       = MAC_Type_Data;
	dh->dh_fc.fc_subtype    = MAC_Subtype_Data;
	
	dh->dh_fc.fc_to_ds      = 0;
	dh->dh_fc.fc_from_ds    = 0;
	dh->dh_fc.fc_more_frag  = 0;
	dh->dh_fc.fc_retry      = 0;
	dh->dh_fc.fc_pwr_mgt    = 0;
	dh->dh_fc.fc_more_data  = 0;
	dh->dh_fc.fc_wep        = 0;
	dh->dh_fc.fc_order      = 0;

	//dh->dh_fc.fc_channel = active_channel_;
//duy
//to be added in check_pktTx
/*
	if(active_channel_ != -1)
		dh->dh_fc.fc_channel = active_channel_;
	else
		dh->dh_fc.fc_channel = DEFAULT_CHANNEL;
*/
//end of duy

	/* store data tx time */
 	ch->txtime() = txtime(ch->size(), dataRate_);



	//duy comments: calculating transmitting time
	if((u_int32_t)ETHER_ADDR(dh->dh_ra) != MAC_BROADCAST) {
		/* store data tx time for unicast packets */
		ch->txtime() = txtime(ch->size(), dataRate_);
		
		dh->dh_duration = usec(txtime(phymib_.getACKlen(), basicRate_)
				       + phymib_.getSIFS());



	} else {
		/* store data tx time for broadcast packets (see 9.6) */
		ch->txtime() = txtime(ch->size(), basicRate_);
		
		dh->dh_duration = 0;
	}


	//delegate to pktTx_
	pktTx_ = p;
}

/* ======================================================================
   Retransmission Routines
   ====================================================================== */
//duy comments:
//call by send_timer() when send mhSend_ expires(after transmission thru calling transmit())
//increase RTS failure counter, ssrc_
//if retry counter exceeds,it would drop the pktTx_, clear retry counter, reset cw
//if not increase cw and start backoff timer
void
Mac802_11::RetransmitRTS()
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In Retransmit\n", Scheduler::instance().clock(), index_);
	#endif
	assert(pktTx_);
	assert(pktRTS_);
	assert(mhBackoff_.busy() == 0);

	macmib_.RTSFailureCount++;


	ssrc_ += 1;			// STA Short Retry Count
		
	if(ssrc_ >= macmib_.getShortRetryLimit()) {
		discard(pktRTS_, DROP_MAC_RETRY_COUNT_EXCEEDED); pktRTS_ = 0;
		/* tell the callback the send operation failed 
		   before discarding the packet */
		hdr_cmn *ch = HDR_CMN(pktTx_);
		if (ch->xmit_failure_) {
                        /*
                         *  Need to remove the MAC header so that 
                         *  re-cycled packets don't keep getting
                         *  bigger.
                         */
			ch->size() -= phymib_.getHdrLen11();
                        ch->xmit_reason_ = XMIT_REASON_RTS;
                        ch->xmit_failure_(pktTx_->copy(),
                                          ch->xmit_failure_data_);
                }
		printf("alert sos, in retransmitrts discard pkts\n");
		discard(pktTx_, DROP_MAC_RETRY_COUNT_EXCEEDED); 
		pktTx_ = 0;
		ssrc_ = 0;
		rst_cw();

	} else {
//duy	
		struct rts_frame *rf;
		rf = (struct rts_frame*)pktRTS_->access(hdr_mac::offset_);
		rf->rf_fc.fc_retry = 1;
		rf->rf_fc.fc_channel = DEFAULT_CHANNEL; 

		inc_cw();

		//double tx_data_time = txtime(pktTx_) + DSSS_MaxPropagationDelay + phymib_.getSIFS() 
		//	+ txtime(phymib_.getACKlen(), basicRate_) + DSSS_MaxPropagationDelay + phymib_.getSIFS();

		if( ( ammac_cur_state_ == AMMAC_CTRL_WIN) || (ammac_cur_state_ == AMMAC_IDLE) )
			mhBackoff_.start(cw_, is_idle());
		else if ((mhWait_.busy() == 0) ) { 
			double commonChan = ammacUsageOnCommonChannel();

			if (commonChan == 0.0) {
				mhBackoff_.start(cw_, is_idle());
			}
			else {
			mhWait_.start(commonChan);
			}


		}
//end of duy
	}
}


//duy comments:
//if pktTx_ holds a broadcast pkt, it would just clear it, reset cw, start backoff timer
//otherwise increase ACK failgure counter, increase ssrc_ or slrc_
//if retry counter exceeds, calling the sending failure callback(hdr_cmn::xmit_failure())
//and clear pktTx, reset retry counter and cw
//if not call sendRTS, increase cw and start backoff


void
Mac802_11::RetransmitDATA()
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In RetransmitDATA\n", Scheduler::instance().clock(), index_);
	#endif
	struct hdr_cmn *ch;
	struct hdr_mac802_11 *mh;
	u_int32_t *rcount, thresh;
	assert(mhBackoff_.busy() == 0);

	assert(pktTx_);
	assert(pktRTS_ == 0);

	ch = HDR_CMN(pktTx_);
	mh = HDR_MAC802_11(pktTx_);

	/*
	 *  Broadcast packets don't get ACKed and therefore
	 *  are never retransmitted.
	 */
	if((u_int32_t)ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
		Packet::free(pktTx_); 
		pktTx_ = 0;

		/*
		 * Backoff at end of TX.
		 */
		rst_cw();
		mhBackoff_.start(cw_, is_idle());

		return;
	}

	macmib_.ACKFailureCount++;

	if((u_int32_t) ch->size() <= macmib_.getRTSThreshold()) {
                rcount = &ssrc_;
               thresh = macmib_.getShortRetryLimit();
        } else {
                rcount = &slrc_;
               thresh = macmib_.getLongRetryLimit();
        }

	(*rcount)++;

	if(*rcount >= thresh) {
		/* IEEE Spec section 9.2.3.5 says this should be greater than
		   or equal */
		macmib_.FailedCount++;
		/* tell the callback the send operation failed 
		   before discarding the packet */
		hdr_cmn *ch = HDR_CMN(pktTx_);
		if (ch->xmit_failure_) {
                        ch->size() -= phymib_.getHdrLen11();
			ch->xmit_reason_ = XMIT_REASON_ACK;
                        ch->xmit_failure_(pktTx_->copy(),
                                          ch->xmit_failure_data_);
                }

		discard(pktTx_, DROP_MAC_RETRY_COUNT_EXCEEDED); 
		pktTx_ = 0;
		*rcount = 0;
		rst_cw();
		active_channel_ = DEFAULT_CHANNEL;
	}
	else {
		struct hdr_mac802_11 *dh;
		dh = HDR_MAC802_11(pktTx_);
		dh->dh_fc.fc_retry = 1;

		active_channel_ = DEFAULT_CHANNEL; //duy

		sendRTS(ETHER_ADDR(mh->dh_ra));
		inc_cw();

//duy
		if((mhWait_.busy() == 0) ) {
			double commonChan = ammacUsageOnCommonChannel();
			if (commonChan == 0.0) {
			;//	mhBackoff_.start(cw_, is_idle());
			}
			else {
				mhWait_.start(commonChan);
			}
		}
//end of duy
	}
}

/* ======================================================================
   Incoming Packet Routines
   ====================================================================== */
void
Mac802_11::send(Packet *p, Handler *h)
{

	double rTime;
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	int dst = (int) ETHER_ADDR(dh->dh_ra);

	u_int8_t subtype =dh->dh_fc.fc_subtype;
	ammac_cur_dest_ = dst;

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In send, would like to send to %d\n", Scheduler::instance().clock(), index_, dst);
	#endif



	EnergyModel *em = netif_->node()->energy_model();
	if (em && em->sleep()) {
		em->set_node_sleep(0);
		em->set_node_state(EnergyModel::INROUTE);
	}
	
	callback_ = h;
	sendDATA(p);
	sendRTS(ETHER_ADDR(dh->dh_ra));

	/*
	 * Assign the data packet a sequence number.
	 */
	dh->dh_scontrol = sta_seqno_++;

	//if dst is broadcasting, silently pass it 
	if((dst == (int) MAC_BROADCAST) && (subtype != MAC_Subtype_ATS)) {
		downtarget_->recv(p->copy(), this);
	
		
		//Packet::free(pktTx_); pktTx_ = 0;
	//	if(callback_) {
	//		Handler *h = callback_;
	//		callback_ = 0;
	//		h->handle((Event*) 0);
	//	}
		return;
	}
	



//	/*
	if( ammac_cur_state_ == AMMAC_DATA_WIN ) { 

		if(mhRdezvous_.busy() == 1) { 
	//		fprintf(stdout, "index %d redez expires %f\n", index_, mhRdezvous_.expire());
			if(mhWait_.busy())
				mhWait_.stop();
			mhWait_.start(mhRdezvous_.expire());
		}
		return;
	}
	else {}
//	*/
	

	

	/*
	 *  If the medium is IDLE, we must wait for a DIFS
	 *  Space before transmitting.
	 */
	if(mhBackoff_.busy() == 0) {
		if(is_idle()) {
			if (mhDefer_.busy() == 0) {
				/*
				 * If we are already deferring, there is no
				 * need to reset the Defer timer.
				 */
				if (bugFix_timer_) {
					 mhBackoff_.start(cw_, is_idle(), 
							  phymib_.getDIFS());
				}
				else {
					rTime = (Random::random() % cw_)
						* (phymib_.getSlotTime());
					mhDefer_.start(phymib_.getDIFS() + 
						       rTime);
				}
			}
		} else {
			/*
			 * If the medium is NOT IDLE, then we start
			 * the backoff timer.
			 */
			mhBackoff_.start(cw_, is_idle());
		}
	}
}


//duy comments
//upper object(probably queue) calls recv
//entry from upper target(a callback handler is given as param)
//to send a packet to MAC
//after MAC transmits this packet successfully, it will
//use this callback handler to inform upper target that MAC is idle
//give another packet if there are pkts buffered

void
Mac802_11::recv(Packet *p, Handler *h)
{
	fprintf(stdout, "%f \tNode:%d In recv,queue size %d \n", Scheduler::instance().clock(), index_, queue_->length());
	struct hdr_cmn *hdr = HDR_CMN(p);
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);

	int ch = (int)dh->dh_fc.fc_channel;
	int dest = (int) ETHER_ADDR(dh->dh_ra);
	int src = (int)ETHER_ADDR(dh->dh_ta); 

	u_int8_t subtype = dh->dh_fc.fc_subtype;


	/*
	 * Sanity Check
	 */
	assert(initialized());

	/*
	 *  Handle outgoing packets.
	 */
	if(hdr->direction() == hdr_cmn::DOWN) {
                send(p, h);
                return;
        }
	
	/*
	 *  Handle incoming packets.
	 *
	 *  We just received the 1st bit of a packet on the network
	 *  interface.
	 *
	 */

	/*
	 *  If the interface is currently in transmit mode, then
	 *  it probably won't even see this packet.  However, the
	 *  "air" around me is BUSY so I need to let the packet
	 *  proceed.  Just set the error flag in the common header
	 *  to that the packet gets thrown away.
	 */
	if(tx_active_ && hdr->error() == 0) {
		hdr->error() = 1;
	}


	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In recv\n", Scheduler::instance().clock(), index_);
	fprintf(stdout, "%f \tNode:%d my state is %s\n", Scheduler::instance().clock(), index_, convertState(rx_state_));
	fprintf(stdout, "%f \tNode:%d sender is %d receiver is %d \n", Scheduler::instance().clock(), index_, src, dest);
	#endif


	
//duy

	
	// if the dest is broadcasting, and this is not a ATS pkt 
	// silently receive it
	if((dest == (int)MAC_BROADCAST) && (subtype != MAC_Subtype_ATS)) {
		uptarget_->recv(p,(Handler*) 0);
		return;
	}
	
	


	if((dh->dh_fc.fc_type == MAC_Type_Data || dh->dh_fc.fc_subtype == MAC_Subtype_ACK) && (dest != (int)MAC_BROADCAST)) {
		if(dest != index_) {
	                      Packet::free(p);
	                      return;
	        }
	}

	//802.11 does not implement noise checking on common channel
	//it only looks at nav which is past history
	//this is a temporary fix 
	//in reality, you have to keep checking the medium
	//is_idle does not work as expected
	if(ch != active_channel_ && dest != index_ && ch == 0) {
		set_nav(usec(txtime(p)));
		Packet::free(p);
		return;
	}


	if(ch == active_channel_ && dest != index_ && ch == 0) {
		set_nav(usec(txtime(p)));
	}
	else if(active_channel_ == -1 && ch == 0) {
		;
	}
	else if(ch == active_channel_) {
		;
	}
	else { //if(ch != active_channel_) {
		Packet::free(p);
		return;
	}

//end of duy




	if(rx_state_ == MAC_IDLE) {
		setRxState(MAC_RECV);
		pktRx_ = p;
		/*
		 * Schedule the reception of this packet, in
		 * txtime seconds.
		 */
		 
		mhRecv_.start(txtime(p));
	} else {

		/*
		 *  If the power of the incoming packet is smaller than the
		 *  power of the packet currently being received by at least
                 *  the capture threshold, then we ignore the new packet.
		 */
		if(pktRx_->txinfo_.RxPr / p->txinfo_.RxPr >= p->txinfo_.CPThresh) {
			capture(p);
		} else {
			collision(p);
		}
	}
}

//duy comments:
//to take care of the receiving pkt
//pkt received and waiting to be processed is held by pktRx_
//pktRx_ whould be checked for collision, error and destination address
//nav would be set, receiving energy is updated
//effective pkt would be forwarded to coreresponding functions
// then mac tate would be set the idle 
void
Mac802_11::recv_timer()
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In recv_timer ammac_state is %d \n", Scheduler::instance().clock(), index_, ammac_cur_state_);
	#endif
	hdr_cmn *ch = HDR_CMN(pktRx_);
	hdr_mac802_11 *mh = HDR_MAC802_11(pktRx_);
	u_int32_t dst = ETHER_ADDR(mh->dh_ra);
	u_int32_t src = ETHER_ADDR(mh->dh_ta); 


	int chan = (int)mh->dh_fc.fc_channel;  //duy
	
	u_int8_t  type = mh->dh_fc.fc_type;
	u_int8_t  subtype = mh->dh_fc.fc_subtype;

	assert(pktRx_);
	assert(rx_state_ == MAC_RECV || rx_state_ == MAC_COLL);
	
        /*
         *  If the interface is in TRANSMIT mode when this packet
         *  "arrives", then I would never have seen it and should
         *  do a silent discard without adjusting the NAV.
         */
        if(tx_active_) {

                Packet::free(pktRx_);
                goto done;
        }

	/*
	 * Handle collisions.
	 */
	if(rx_state_ == MAC_COLL) {

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d is in collision, active_channel is %d selected is %d\n", Scheduler::instance().clock(), index_, active_channel_, selected_channel_);
	for(int i= NUM_OF_CHANNELS-1; i>=0; i--)
		fprintf(stdout, "%f \t***channel time usage %f by the pair %d and %d on channel %d\n", Scheduler::instance().clock(), ammac_cul_[i], ammac_cul_node_[i][0],ammac_cul_node_[i][1], i);
	#endif

		discard(pktRx_, DROP_MAC_COLLISION);		
		set_nav(usec(phymib_.getEIFS()));
		goto done;
	}

	/*
	 * Check to see if this packet was received with enough
	 * bit errors that the current level of FEC still could not
	 * fix all of the problems - ie; after FEC, the checksum still
	 * failed.
	 */
	if( ch->error() ) {

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d is packet error, set_nav on chan %d\n", Scheduler::instance().clock(), index_,chan);
	#endif

		Packet::free(pktRx_);
		set_nav(usec(phymib_.getEIFS()));
		goto done;
	}

	/*
	 * IEEE 802.11 specs, section 9.2.5.6
	 *	- update the NAV (Network Allocation Vector)
	 */
	if(dst != (u_int32_t)index_) {
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d is not the intended receiver chan is %d\n", Scheduler::instance().clock(), index_, chan);
	#endif
		//update channel usage list
		//exclusive for dst != index
		ammacUpdateChansInfo(); //duy

		if(chan == 0)
			set_nav(mh->dh_duration);
	}

        /* tap out - */
        if (tap_ && type == MAC_Type_Data &&
            MAC_Subtype_Data == subtype ) 
		tap_->tap(pktRx_);
	/*
	 * Adaptive Fidelity Algorithm Support - neighborhood infomation 
	 * collection
	 *
	 * Hacking: Before filter the packet, log the neighbor node
	 * I can hear the packet, the src is my neighbor
	 */
	if (netif_->node()->energy_model() && 
	    netif_->node()->energy_model()->adaptivefidelity()) {
		src = ETHER_ADDR(mh->dh_ta);
		netif_->node()->energy_model()->add_neighbor(src);
	}
	/*
	 * Address Filtering
	 */
	if(dst != (u_int32_t)index_ && dst != MAC_BROADCAST) {
		/*
		 *  We don't want to log this event, so we just free
		 *  the packet instead of calling the drop routine.
		 */
		discard(pktRx_, "---");
		goto done;
	}

	switch(type) {

	case MAC_Type_Management:
		discard(pktRx_, DROP_MAC_PACKET_ERROR);
		goto done;
	case MAC_Type_Control:
		switch(subtype) {
		case MAC_Subtype_RTS:
			recvRTS(pktRx_);
			break;
		case MAC_Subtype_CTS:
			recvCTS(pktRx_);
			break;
		case MAC_Subtype_ACK:
			recvACK(pktRx_);
			break;
		case MAC_Subtype_ATS:
			recvATS(pktRx_);
			break;
		default:
			fprintf(stderr,"recvTimer1:Invalid MAC Control Subtype %x\n",
				subtype);
			exit(1);
		}
		break;
	case MAC_Type_Data:
		switch(subtype) {
		case MAC_Subtype_Data:
			recvDATA(pktRx_);
			break;
		default:
			fprintf(stderr, "recv_timer2:Invalid MAC Data Subtype %x\n",
				subtype);
			exit(1);
		}
		break;
	default:
		fprintf(stderr, "recv_timer3:Invalid MAC Type %x\n", subtype);
		exit(1);
	}
 done:
	pktRx_ = 0;
	rx_resume();
}


//duy comments:
//called by recv_timer when received pkt held in pktRx_ is RTS
//calls sendCTS which sets pktCTRL_ to CTS pkt
//calls tx_resume
void
Mac802_11::recvRTS(Packet *p)
{

	double new_ctrl_time;

	struct rts_frame *rf = (struct rts_frame*)p->access(hdr_mac::offset_);

	double nego_time = txtime(phymib_.getCTSlen(), basicRate_)
				+ DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getATSlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;

	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In recvRTS data dur %f ammac_time is%f\n", Scheduler::instance().clock(), index_, sec_uint(rf->rf_duration_data_win), mhCtrl_.expire() + NOW);
	#endif

	//grab the data time from it
	set_data_transfer(sec_uint(rf->rf_duration_data_win)); //duy

/*
	if(sec_uint(rf->rf_duration_data_win) <= 0) {
		printf("error in recvCTS\n");
		return;
	}
	*/


	if(tx_state_ != MAC_IDLE) {
		discard(p, DROP_MAC_BUSY);
		return;
	}

	/*
	 *  If I'm responding to someone else, discard this RTS.
	 */
	if(pktCTRL_) {
		discard(p, DROP_MAC_BUSY);
		return;
	}
//duy
	//you're scheduled
	//drop it
	if(selected_channel_ >= 0) {
		#ifdef DEBUG
		fprintf(stdout, "I am scheduled, exitin\n");
		#endif

		discard(p, DROP_MAC_BUSY);
		return;
	}
	
	


	switch(ammac_cur_state_) {
	
	case AMMAC_IDLE:

		new_ctrl_time = sec_uint(rf->rf_duration_ctrl_win) + NOW - txtime(phymib_.getRTSlen(), basicRate_) - DSSS_MaxPropagationDelay;
		
		if((new_ctrl_time - nego_time) <= NOW) 
			printf("love\n");
		else {
			ammac_ctrl_time_ = new_ctrl_time;
			if(mhCtrl_.busy()) 
				mhCtrl_.stop();
			mhCtrl_.start(ammac_ctrl_time_ - NOW);
		}
		
		/*
		if(( mhCtrl_.expire() - nego_time) <  0) {
			if( (new_ctrl_time - nego_time) >= 0.0) {
				ammac_ctrl_time_ = new_ctrl_time + NOW;
				if(mhCtrl_.busy())
					mhCtrl_.stop();
				mhCtrl_.start(new_ctrl_time);
			} else 
				return;

		} 
		else ;
		*/

		ammac_cur_state_ = AMMAC_CTRL_WIN;
		//ammac_ctrl_time_ = new_ctrl_time;

		break;

	case AMMAC_CTRL_WIN:

		new_ctrl_time = sec_uint(rf->rf_duration_ctrl_win)  + NOW - txtime(phymib_.getRTSlen(), basicRate_) - DSSS_MaxPropagationDelay;

		if((new_ctrl_time - nego_time) <= NOW) 
			printf("love\n");
		else {
			ammac_ctrl_time_ = new_ctrl_time;
			if(mhCtrl_.busy()) 
				mhCtrl_.stop();
			mhCtrl_.start(ammac_ctrl_time_ - NOW);
		}

		

		/*
		if((mhCtrl_.expire() - nego_time) < 0) {
			if(new_ctrl_time - nego_time >= 0.0) {
				ammac_ctrl_time_ = new_ctrl_time + NOW;
				if(mhCtrl_.busy())
					mhCtrl_.stop();
				mhCtrl_.start(new_ctrl_time);
			} else
				return;
		}
		else ;
		*/

		break;
	case AMMAC_DATA_WIN:
		discard(p, DROP_MAC_BUSY);
		return;
	default:
		exit(1);

	}	

	//get list of channels from RTS(use it in check_pktCTRL)
	for( int i =0; i < NUM_OF_CHANNELS; i++) {
		ammac_recved_fcl_[i] = rf->rf_channel_list[i];
	}

//end of duy

	sendCTS(ETHER_ADDR(rf->rf_ta), rf->rf_duration);

	/*
	 *  Stop deferring - will be reset in tx_resume().
	 */
	if(mhDefer_.busy()) mhDefer_.stop();

	tx_resume();

	mac_log(p);
}

/*
 * txtime()	- pluck the precomputed tx time from the packet header
 */
double
Mac802_11::txtime(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	double t = ch->txtime();
	if (t < 0.0) {
		drop(p, "XXX");
 		exit(1);
	}
	return t;
}

 
/*
 * txtime()	- calculate tx time for packet of size "psz" bytes 
 *		  at rate "drt" bps
 */
double
Mac802_11::txtime(double psz, double drt)
{
	double dsz = psz - phymib_.getPLCPhdrLen();
        int plcp_hdr = phymib_.getPLCPhdrLen() << 3;	
	int datalen = (int)dsz << 3;
	double t = (((double)plcp_hdr)/phymib_.getPLCPDataRate())
                                       + (((double)datalen)/drt);
	return(t);
}


//duy comments:
// called by recv_timer() when a CTS packet is received
//it clears pktRTS_ which holds RTS pkt for this CTS
//stops sending timer mhSend_ if it is set
//resets STA short retry counter ssrc_
//and calls tx_resume()

void
Mac802_11::recvCTS(Packet *p)
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In recvCTS\n", Scheduler::instance().clock(), index_);
	#endif
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);

	int dst = (int) ETHER_ADDR(dh->dh_ta);

	if(tx_state_ != MAC_RTS) {
		discard(p, DROP_MAC_INVALID_STATE);
		return;
	}

	struct cts_frame *cf = (struct cts_frame*)pktRx_->access(hdr_mac::offset_);

//duy
	double new_ctrl_time, data_time;


	set_data_transfer(sec_uint(cf->cf_duration_data_win));
	selected_channel_ = cf->cf_selected_channel;


	if(selected_channel_ < 0) {
		new_ctrl_time = sec_uint(cf->cf_duration_ctrl_win) - txtime(phymib_.getCTSlen(), basicRate_) - DSSS_MaxPropagationDelay;
		data_time =  txtime(phymib_.getATSlen(), basicRate_) + DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ sec_uint(cf->cf_duration_data_win );

		ammacUpdateMyTimers(new_ctrl_time, data_time);

		mac_log(p);
		tx_resume();

		return;
	}

	new_ctrl_time= sec_uint(cf->cf_duration_ctrl_win) - txtime(phymib_.getCTSlen(), basicRate_) - DSSS_MaxPropagationDelay;
	data_time = txtime(phymib_.getATSlen(), basicRate_) + DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ sec_uint(cf->cf_duration_data_win );
	ammacUpdateMyTimers(new_ctrl_time,data_time);

//end of duy
		

	assert(pktRTS_);
	Packet::free(pktRTS_); pktRTS_= 0;

	assert(pktTx_);

	mhSend_.stop();

	/*
	 * The successful reception of this CTS packet implies
	 * that our RTS was successful. 
	 * According to the IEEE spec 9.2.5.3, you must 
	 * reset the ssrc_, but not the congestion window.
	 */
	ssrc_ = 0;


	sendATS(dst); //duy
	if(mhDefer_.busy()) mhDefer_.stop();
	

	tx_resume();

	mac_log(p);
}


//duy comments:
//called by recv_timer
//if dst is not a broadcast addr(it is a unicast pkt)
//ack would be sent through calling sendACK() & tx_resume would be called
// the received data pkt would be forwarded to up-target if it is not dropped
//(due to a control pkt held in pktCTRL_ if another transmission is on the way)

void
Mac802_11::recvDATA(Packet *p)
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d In recvDATA\n", Scheduler::instance().clock(), index_);
	fprintf(stdout, "%f \tNode:%d my data time is %f\n", Scheduler::instance().clock(), index_,ammac_data_time_);
	#endif
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);
	u_int32_t dst, src, size;
	struct hdr_cmn *ch = HDR_CMN(p);

	dst = ETHER_ADDR(dh->dh_ra);
	src = ETHER_ADDR(dh->dh_ta);
	size = ch->size();
	/*
	 * Adjust the MAC packet size - ie; strip
	 * off the mac header
	 */
	ch->size() -= phymib_.getHdrLen11();
	ch->num_forwards() += 1;

	/*
	 *  If we sent a CTS, clean up...
	 */
	if(dst != MAC_BROADCAST) {
		#ifdef DEBUG
		printf("it is a unicast, state is %s\n", convertState(tx_state_));
		#endif
		if(size >= macmib_.getRTSThreshold()) {
		
			if (tx_state_ == MAC_CTS) {
//			if (tx_state_ == MAC_WAIT) {
				assert(pktCTRL_);
				Packet::free(pktCTRL_); pktCTRL_ = 0;
				mhSend_.stop();
			//	setTxState(MAC_IDLE);
				 
			} else if(tx_state_ == MAC_IDLE) {
				;
			}
			else {
				discard(p, DROP_MAC_BUSY);
				return;
			}
			sendACK(src);

//	double delay= txtime(phymib_.getACKlen(), basicRate_);

//	fprintf(stdout, "my ammac_data_time_ is %f \n", ammac_data_time_);
	/*
	//dduk
	if(NOW < ammac_data_time_ - delay) {
			
		
		p = ll_->ifq()->getNextGoodPkt(ammac_cur_dest_);
		if(p == NULL) {
			tx_resume();
			return;
		}

		struct hdr_mac802_11* dh = HDR_MAC802_11(p);
		sendDATA(p);

		dh->dh_scontrol = sta_seqno_++;

		printf("I want to send again\n");

		//tbresolve
		if(tx_state_ != MAC_WAIT) 
			setTxState(MAC_IDLE);
		if(mhDefer_.busy() == 0)
		//	mhDefer_.start(phymib_.getDIFS());
			mhDefer_.start(txtime(pktTx_));
	}*/
		 tx_resume();
		
		} else {
			/*
			 *  We did not send a CTS and there's no
			 *  room to buffer an ACK.
			 */
		printf("no room to buffer ACK, we did not send a CTS\n");
			if(pktCTRL_) {
				discard(p, DROP_MAC_BUSY);
				return;
			}
			sendACK(src);
			if(mhSend_.busy() == 0)
				tx_resume();
		}
	}
	
	/* ============================================================
	   Make/update an entry in our sequence number cache.
	   ============================================================ */

	/* Changed by Debojyoti Dutta. This upper loop of if{}else was 
	   suggested by Joerg Diederich <dieder@ibr.cs.tu-bs.de>. 
	   Changed on 19th Oct'2000 */

        if(dst != MAC_BROADCAST) {
                if (src < (u_int32_t) cache_node_count_) {
                        Host *h = &cache_[src];

                        if(h->seqno && h->seqno == dh->dh_scontrol) {
                                discard(p, DROP_MAC_DUPLICATE);
                                return;
                        }
                        h->seqno = dh->dh_scontrol;
                } else {
			static int count = 0;
			if (++count <= 10) {
				printf ("MAC_802_11: accessing MAC cache_ array out of range (src %u, dst %u, size %d)!\n", src, dst, cache_node_count_);
				if (count == 10)
					printf ("[suppressing additional MAC cache_ warnings]\n");
			};
		};
	}

	/*
	 *  Pass the packet up to the link-layer.
	 *  XXX - we could schedule an event to account
	 *  for this processing delay.
	 */
	
	/* in BSS mode, if a station receives a packet via
	 * the AP, and higher layers are interested in looking
	 * at the src address, we might need to put it at
	 * the right place - lest the higher layers end up
	 * believing the AP address to be the src addr! a quick
	 * grep didn't turn up any higher layers interested in
	 * the src addr though!
	 * anyway, here if I'm the AP and the destination
	 * address (in dh_3a) isn't me, then we have to fwd
	 * the packet; we pick the real destination and set
	 * set it up for the LL; we save the real src into
	 * the dh_3a field for the 'interested in the info'
	 * receiver; we finally push the packet towards the
	 * LL to be added back to my queue - accomplish this
	 * by reversing the direction!*/

	if ((bss_id() == addr()) && ((u_int32_t)ETHER_ADDR(dh->dh_ra)!= MAC_BROADCAST)&& ((u_int32_t)ETHER_ADDR(dh->dh_3a) != ((u_int32_t)addr()))) {
		struct hdr_cmn *ch = HDR_CMN(p);
		u_int32_t dst = ETHER_ADDR(dh->dh_3a);
		u_int32_t src = ETHER_ADDR(dh->dh_ta);
		/* if it is a broadcast pkt then send a copy up
		 * my stack also
		 */
		if (dst == MAC_BROADCAST) {
			uptarget_->recv(p->copy(), (Handler*) 0);
		}

		ch->next_hop() = dst;
		STORE4BYTE(&src, (dh->dh_3a));
		ch->addr_type() = NS_AF_ILINK;
		ch->direction() = hdr_cmn::DOWN;
	}

	uptarget_->recv(p, (Handler*) 0);
}

//duy

void 
Mac802_11::recvATS(Packet *p)
{
	#ifdef DEBUG
	fprintf(stdout, "%f \tNode:%d in recvATS\n", Scheduler::instance().clock(), index_);
	fprintf(stdout, "%f \tNode:%d in recvATS current state is %s\n", Scheduler::instance().clock(), index_, convertState(tx_state_));
	#endif

	if(tx_state_ != MAC_CTS) {
		discard(p, DROP_MAC_INVALID_STATE);
		return;
	}


	struct ats_frame *rf = (struct ats_frame*)pktRx_->access(hdr_mac::offset_);
	set_data_transfer(sec_uint(rf->ats_duration_data_win));

	assert(pktTx_);
	mhSend_.stop();
	ssrc_ = 0;


//	mhSend_.start(get_data_transfer() + phymib_.getSIFS());

//	assert(pktCTRL_);
//	Packet::free(pktCTRL_); pktCTRL_ = 0;

//	setTxState(MAC_WAIT);


	if(mhRdezvous_.busy() )
		mhRdezvous_.stop();
	rdezvousHandler();

	mac_log(p);
}

//end of duy

//duy comments
// called by recv_timer() if ack pkt is received.
//resets sta retry counter ssrc_ or slrc_, the size of contention window
// clears pkt held in pktTx_ then calls tx_resume()
void
Mac802_11::recvACK(Packet *p)
{	
	#ifdef DEBUG
	fprintf(stdout, "%f Node:%d in recvACK\n", Scheduler::instance().clock(), index_);
	fprintf(stdout, "data transmission takes %f\n", NOW - get_stop_watch()) ;
	fprintf(stdout, "%f \tNode:%d my data time is %f\n", Scheduler::instance().clock(), index_,ammac_data_time_);
	#endif

	if(tx_state_ != MAC_SEND) {
		discard(p, DROP_MAC_INVALID_STATE);
		return;
	}
	assert(pktTx_);

	mhSend_.stop();

	/*
	 * The successful reception of this ACK packet implies
	 * that our DATA transmission was successful.  Hence,
	 * we can reset the Short/Long Retry Count and the CW.
	 *
	 * need to check the size of the packet we sent that's being
	 * ACK'd, not the size of the ACK packet.
	 */
	if((u_int32_t) HDR_CMN(pktTx_)->size() <= macmib_.getRTSThreshold())
		ssrc_ = 0;
	else
		slrc_ = 0;
	rst_cw();
	Packet::free(pktTx_); 
	pktTx_ = 0;


	mac_log(p);

//	double	data_tx_time =  /*get_data_transfer() */
//					DSSS_MaxPropagationDelay + phymib_.getSIFS()
//					+ txtime(phymib_.getACKlen(), basicRate_)
//					+ DSSS_MaxPropagationDelay;
	//you're receving an ack, and it is still within the data time
	// get the next good packet from queue
	double delay = DSSS_MaxPropagationDelay + phymib_.getSIFS()
				+ txtime(phymib_.getACKlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;

//	double data_limit =  .002630;
//	double data_limit =  .000694;
	double data_limit =  .000424;

	fprintf(stdout, "my ammac_data_time_ is %f \n", ammac_data_time_);
	//duk
	if(NOW < ammac_data_time_ - data_limit) {
			
		
		
		p = ll_->ifq()->getNextGoodPkt(ammac_cur_dest_);
		if(p == NULL) {
			tx_resume();
			return;
		}

		printf("xxx I am sending again \n");
		struct hdr_mac802_11* dh = HDR_MAC802_11(p);
		sendDATA(p);

		dh->dh_scontrol = sta_seqno_++;

		printf("I want to send again\n");

		if(tx_state_ != MAC_WAIT) 
			setTxState(MAC_IDLE);
		if(mhDefer_.busy() == 0)
			mhDefer_.start(phymib_.getDIFS());
			
	}
	else {


	/*
	 * Backoff before sending again.
	 */
	assert(mhBackoff_.busy() == 0);

	if(past_active_channel_ != 0) {
		//error converting u_int32_t from double
		//	cw_ = cw_ + .01;
		past_active_channel_ = -1;

	}

	mhBackoff_.start(cw_, is_idle());
	tx_resume();

	}


}
