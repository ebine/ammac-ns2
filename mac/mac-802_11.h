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
 * $Header: /cvsroot/nsnam/ns-2/mac/mac-802_11.h,v 1.27 2006/02/22 13:25:43 mahrenho Exp $
 *
 * Ported from CMU/Monarch's code, nov'98 -Padma.
 * wireless-mac-802_11.h
 */

#ifndef ns_mac_80211_h
#define ns_mac_80211_h

// Added by Sushmita to support event tracing (singal@nunki.usc.edu)
#include "address.h"
#include "ip.h"

#include "mac.h" 	//duy

#include "mac-timers.h"
#include "marshall.h"
#include <math.h>
#include <stddef.h>

class EventTrace;

#define GET_ETHER_TYPE(x)		GET2BYTE((x))
#define SET_ETHER_TYPE(x,y)            {u_int16_t t = (y); STORE2BYTE(x,&t);}

/* ======================================================================
   Frame Formats
   ====================================================================== */

#define NUM_OF_CHANNELS		3	//duy


#define	DEFAULT_CHANNEL 0

//duy

//AM_MAC states
#define AMMAC_IDLE	0
#define AMMAC_CTRL_WIN	1
#define AMMAC_DATA_WIN	2

//end of duy


#define	MAC_ProtocolVersion	0x00

#define MAC_Type_Management	0x00
#define MAC_Type_Control	0x01
#define MAC_Type_Data		0x02
#define MAC_Type_Reserved	0x03

#define MAC_Subtype_RTS		0x0B
#define MAC_Subtype_CTS		0x0C
#define MAC_Subtype_ACK		0x0D
#define MAC_Subtype_ATS		0x0E
#define MAC_Subtype_CRAP	0x0F
#define MAC_Subtype_Data	0x00


struct frame_control {
	u_char		fc_subtype		: 4;
	u_char		fc_type			: 2;
	u_char		fc_protocol_version	: 2;

	u_char		fc_order		: 1;
	u_char		fc_wep			: 1;
	u_char		fc_more_data		: 1;
	u_char		fc_pwr_mgt		: 1;
	u_char		fc_retry		: 1;
	u_char		fc_more_frag		: 1;
	u_char		fc_from_ds		: 1;
	u_char		fc_to_ds		: 1;

	u_char		fc_channel		: 4;  // duy
};

struct rts_frame {
	struct frame_control	rf_fc;
	u_int16_t		rf_duration;		
	u_char			rf_ra[ETHER_ADDR_LEN];
	u_char			rf_ta[ETHER_ADDR_LEN];
	u_char			rf_fcs[ETHER_FCS_LEN];

//duy
	u_int32_t		rf_duration_ctrl_win;	
	u_int32_t		rf_duration_data_win;	

	int			rf_channel_list[NUM_OF_CHANNELS]; 
	int 			rf_initiator;
//end of duy	

};

struct cts_frame {
	struct frame_control	cf_fc;
	u_int16_t		cf_duration;
	u_char			cf_ra[ETHER_ADDR_LEN];
	u_char			cf_ta[ETHER_ADDR_LEN];
	u_char			cf_fcs[ETHER_FCS_LEN];

//duy
	u_int32_t		cf_duration_ctrl_win;	
	u_int32_t		cf_duration_data_win;	
	
	int			cf_selected_channel; 

//end of duy

};

//duy
struct ats_frame {
	struct frame_control	ats_fc;
	u_int16_t		ats_duration;
	u_char			ats_ra[ETHER_ADDR_LEN];
	u_char			ats_ta[ETHER_ADDR_LEN];
	u_char			ats_fcs[ETHER_FCS_LEN];

	u_int32_t		ats_duration_ctrl_win;
	u_int32_t		ats_duration_data_win;

	int			ats_confirmed_channel;
};
//end of duy 


struct ack_frame {
	struct frame_control	af_fc;
	u_int16_t		af_duration;
	u_char			af_ra[ETHER_ADDR_LEN];
	u_char			af_fcs[ETHER_FCS_LEN];
};

// XXX This header does not have its header access function because it shares
// the same header space with hdr_mac.
struct hdr_mac802_11 {
	struct frame_control	dh_fc;
	u_int16_t		dh_duration;
	u_char                  dh_ra[ETHER_ADDR_LEN];
        u_char                  dh_ta[ETHER_ADDR_LEN];
        u_char                  dh_3a[ETHER_ADDR_LEN];
	u_int16_t		dh_scontrol;
	u_char			dh_body[1]; // size of 1 for ANSI compatibility
};


/* ======================================================================
   Definitions
   ====================================================================== */

/* Must account for propagation delays added by the channel model when
 * calculating tx timeouts (as set in tcl/lan/ns-mac.tcl).
 *   -- Gavin Holland, March 2002
 */
#define DSSS_MaxPropagationDelay        0.000002        // 2us   XXXX

//ddn physical layer management information base(MIB)
class PHY_MIB {
public:
	PHY_MIB(Mac802_11 *parent);

	inline u_int32_t getCWMin() { return(CWMin); }
        inline u_int32_t getCWMax() { return(CWMax); }
	inline double getSlotTime() { return(SlotTime); }
	inline double getSIFS() { return(SIFSTime); }
	inline double getPIFS() { return(SIFSTime + SlotTime); }
	inline double getDIFS() { return(SIFSTime + 2 * SlotTime); }
	inline double getEIFS() {
		// see (802.11-1999, 9.2.10)
		return(SIFSTime + getDIFS()
                       + (8 *  getACKlen())/PLCPDataRate);
	}
	inline u_int32_t getPreambleLength() { return(PreambleLength); }
	inline double getPLCPDataRate() { return(PLCPDataRate); }
	
	inline u_int32_t getPLCPhdrLen() {
		return((PreambleLength + PLCPHeaderLength) >> 3);
	}

	inline u_int32_t getHdrLen11() {
		return(getPLCPhdrLen() + offsetof(struct hdr_mac802_11, dh_body[0])
                       + ETHER_FCS_LEN);
	}
	
	inline u_int32_t getRTSlen() {
		return(getPLCPhdrLen() + sizeof(struct rts_frame));
	}
	
	inline u_int32_t getCTSlen() {
		return(getPLCPhdrLen() + sizeof(struct cts_frame));
	}
	
	// duy
	inline u_int32_t getATSlen() {
		return(getPLCPhdrLen() + sizeof(struct ats_frame));
	}
	//end of duy

	inline u_int32_t getACKlen() {
		return(getPLCPhdrLen() + sizeof(struct ack_frame));
	}

 private:




	u_int32_t	CWMin;
	u_int32_t	CWMax;
	double		SlotTime;
	double		SIFSTime;
	u_int32_t	PreambleLength;
	u_int32_t	PLCPHeaderLength;
	double		PLCPDataRate;
};


/*
 * IEEE 802.11 Spec, section 11.4.4.2
 *      - default values for the MAC Attributes
 */
#define MAC_FragmentationThreshold	2346		// bytes
#define MAC_MaxTransmitMSDULifetime	512		// time units
#define MAC_MaxReceiveLifetime		512		// time units

class MAC_MIB {
public:

	MAC_MIB(Mac802_11 *parent);

private:
	u_int32_t	RTSThreshold;
	u_int32_t	ShortRetryLimit;
	u_int32_t	LongRetryLimit;
public:
	u_int32_t	FailedCount;	
	u_int32_t	RTSFailureCount;
	u_int32_t	ACKFailureCount;

 public:
       inline u_int32_t getRTSThreshold() { return(RTSThreshold);}
       inline u_int32_t getShortRetryLimit() { return(ShortRetryLimit);}
       inline u_int32_t getLongRetryLimit() { return(LongRetryLimit);}
};


/* ======================================================================
   The following destination class is used for duplicate detection.
   ====================================================================== */
class Host {
public:
	LIST_ENTRY(Host) link;
	u_int32_t	index;
	u_int32_t	seqno;
};


/* ======================================================================
   The actual 802.11 MAC class.
   ====================================================================== */
class Mac802_11 : public Mac {
	friend class DeferTimer;

	friend class WaitTimer; 	//duy
	friend class RdezvousTimer; 	//duy
	friend class CtrlTimer;		//duy 	

	friend class BackoffTimer;
	friend class IFTimer;
	friend class NavTimer;
	friend class RxTimer;
	friend class TxTimer;

public:
	Mac802_11();
	void		recv(Packet *p, Handler *h);
	inline int	hdr_dst(char* hdr, int dst = -2);
	inline int	hdr_src(char* hdr, int src = -2);
	inline int	hdr_type(char* hdr, u_int16_t type = 0);
	
	inline int bss_id() { return bss_id_; }
	
	// Added by Sushmita to support event tracing
        void trace_event(char *, Packet *);
        EventTrace *et_;

//duy
	void	set_stop_watch(double ttime) { stop_watch_ = ttime;} 
	double	get_stop_watch() { return stop_watch_;} 

	void	set_data_transfer(double ttime) { data_transfer_time_ = ttime;} 
	double	get_data_transfer() { return data_transfer_time_;} 

//end of duy

protected:
	void	backoffHandler(void);
	void	deferHandler(void);
	void	navHandler(void);
	void	recvHandler(void);
	void	sendHandler(void);
	void	txHandler(void);

//duy
	void	waitHandler(void);	
	void	rdezvousHandler(void);	
	void	ctrlHandler(int);

//end of duy

private:
	int		command(int argc, const char*const* argv);

	/* In support of bug fix described at
	 * http://www.dei.unipd.it/wdyn/?IDsezione=2435	 
	 */
	int bugFix_timer_;

	/*
	 * Called by the timers.
	 */
	void		recv_timer(void);
	void		send_timer(void);
	int		check_pktCTRL();
	int		check_pktRTS();
	int		check_pktATS();		//duy
	int		check_pktTx();

	/*
	 * Packet Transmission Functions.
	 */
	void	send(Packet *p, Handler *h);
	void 	sendRTS(int dst);
	void	sendCTS(int dst, double duration);
	void	sendATS(int dst); //duy

	void	sendACK(int dst);
	void	sendDATA(Packet *p);
	void	RetransmitRTS();
	void	RetransmitDATA();

	/*
	 * Packet Reception Functions.
	 */
	void	recvRTS(Packet *p);
	void	recvCTS(Packet *p);
	void	recvATS(Packet *p); //duy
	void	recvACK(Packet *p);
	void	recvDATA(Packet *p);

	void		capture(Packet *p);
	void		collision(Packet *p);
	void		discard(Packet *p, const char* why);
	void		rx_resume(void);
	void		tx_resume(void);

	inline int	is_idle(void);

	/*
	 * Debugging Functions.
	 */
	void		trace_pkt(Packet *p);
	void		dump(char* fname);

	inline int initialized() {	
		return (cache_ && logtarget_
                        && Mac::initialized());
	}

	inline void mac_log(Packet *p) {
                logtarget_->recv(p, (Handler*) 0);
        }

	double txtime(Packet *p);
	double txtime(double psz, double drt);
	double txtime(int bytes) { /* clobber inherited txtime() */ abort(); return 0;}

	inline void transmit(Packet *p, double timeout);
	inline void checkBackoffTimer(void);
	inline void postBackoff(int pri);
	inline void setRxState(MacState newState);
	inline void setTxState(MacState newState);

	char * convertState(MacState newState);  //duy for debugging


	inline void inc_cw() {
		cw_ = (cw_ << 1) + 1;
		if(cw_ > phymib_.getCWMax())
			cw_ = phymib_.getCWMax();
	}
	inline void rst_cw() { cw_ = phymib_.getCWMin(); }


	inline double sec(double t) { return(t *= 1.0e-6); }

	inline u_int16_t usec(double t) {
		u_int16_t us = (u_int16_t)floor((t *= 1e6) + 0.5);
		return us;
	}

	//duy 

	//given uint of micro seconds, return equi value in seconds
	inline double sec_uint(u_int32_t t) {
		double us = t * 1.0e-6;
		return (us);
	}

	//comments give double value of seconds, return micro seconds
	inline u_int32_t usec_32(double t) {
		u_int32_t us = (u_int32_t)floor((t *= 1e6) + 0.5);
		return us;
	}
	//end of duy

	
	inline void set_nav(u_int16_t us) {
		double now = Scheduler::instance().clock();
		double t =  us * 1e-6;
		if((now + t) > nav_) {
			nav_ = now + t;
			if(mhNav_.busy())
				mhNav_.stop();
			mhNav_.start(t);
		}
	}


	//duy
	int ammacBuildFreeChannelList(void);
	int ammacIsRcvrBusy(int);
	int ammacSelectChannel(void);
	void ammacUpdateChansInfo(void);
	void ammacUpdateMyTimers(double, double);
	void ammacClearVars(void);
	void ammacUpdateChannelUsageList(double, int, int, int);
	void ammacClearChannelState(void);
	double ammacUsageOnCommonChannel(void);
	double ammacUsageChanStats(void);
	double ammacFollowInitiator();
	//end duy 


protected:
	PHY_MIB         phymib_;
        MAC_MIB         macmib_;

       /* the macaddr of my AP in BSS mode; for IBSS mode
        * this is set to a reserved value IBSS_ID - the
        * MAC_BROADCAST reserved value can be used for this
        * purpose
        */
       int     bss_id_;
       enum    {IBSS_ID=MAC_BROADCAST};


private:

	//duy comments: transmission rate for control packets
	double		basicRate_;

	//duy comments: transmission rate for mac layer data packets
 	double		dataRate_;
	
	/*
	 * Mac Timers
	 */

	 //duy comments: keeps tracks of how long the interface will be in
	 //transmit mode.  The handler is txHandler()
	IFTimer		mhIF_;		// interface timer


	//duy comments: started at the reception of a pkt for length of time
	//indicated in the dur field of MAC header
	// and call navHandler on expiration
	NavTimer	mhNav_;		// NAV timer

	//duy comments: started when first bit of a pkt is received and set for 
	//the length of time the packet will require to be completely received
	//In case of pkt collision, the receive timer is reset to expire at the end
	//of the last colliding packet
	//timer indirectly calls recv_timer() on expiration by calling recvHandler()
	RxTimer		mhRecv_;		// incoming packets

	//duy comments: indicats the time which an ACK/CTS should have received
	//TxTimer(mhSend_) is started when a pkt is transmitted by the
	//transmit function.   The timer is stopped when a CTS,data, ACK is received
	//on expiration send_timer is indirectly called by sendHandler()
	TxTimer		mhSend_;		// outgoing packets

	DeferTimer	mhDefer_;	// defer timer
	BackoffTimer	mhBackoff_;	// backoff timer

//duy
	WaitTimer	mhWait_; 
	RdezvousTimer	mhRdezvous_; 
	CtrlTimer	mhCtrl_;

//end of duy


	/* ============================================================
	   Internal MAC State
	   ============================================================ */

	//duy

	double 		data_transfer_time_;
	double 		stop_watch_;
	int		active_channel_;	
	int		past_active_channel_;	
	int 		send_again_;


	int 		ammac_cur_state_;
	double		ammac_ctrl_time_;
	double		ammac_data_time_;



	int		fcl_[NUM_OF_CHANNELS];  //free channel list
	int		ammac_recved_fcl_[NUM_OF_CHANNELS];
	double		ammac_cul_[NUM_OF_CHANNELS];
//	double		ammac_cdul_[NUM_OF_CHANNELS]; //current data usage list


			//for the pair using that channel
	int		ammac_cul_node_[NUM_OF_CHANNELS][2];

	int 		selected_channel_;


	int 		ammac_cur_dest_;


//	int		ammac_total_pkts_in_next_slot_;
//	int		ammac_total_nodes_in_next_slot_;

	double		nav_;		// Network Allocation Vector

	//end of duy

	MacState	rx_state_;	// incoming state (MAC_RECV or MAC_IDLE)
	MacState	tx_state_;	// outgoint state
	int		tx_active_;	// transmitter is ACTIVE 

	Packet          *eotPacket_;    // copy for eot callback

	Packet		*pktRTS_;	// outgoing RTS packet
	Packet		*pktCTRL_;	// outgoing non-RTS packet
	Packet		*pktATS_;	//duy

	u_int32_t	cw_;		// Contention Window
	u_int32_t	ssrc_;		// STA Short Retry Count
	u_int32_t	slrc_;		// STA Long Retry Count

	int		min_frame_len_;

	NsObject*	logtarget_;
	NsObject*       EOTtarget_;     // given a copy of packet at TX end




	/* ============================================================
	   Duplicate Detection state
	   ============================================================ */
	u_int16_t	sta_seqno_;	// next seqno that I'll use
	int		cache_node_count_;
	Host		*cache_;
};

#endif /* __mac_80211_h__ */

