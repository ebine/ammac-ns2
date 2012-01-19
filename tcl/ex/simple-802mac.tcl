# Copyright (c) 1997 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by the Computer Systems
#      Engineering Group at Lawrence Berkeley Laboratory.
# 4. Neither the name of the University nor of the Laboratory may be used
#    to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# simple-802mac.tcl
# A simple example for wireless simulation

# ======================================================================
# Define options
# ======================================================================
set val(chan)           Channel/WirelessChannel    ;# channel type
set val(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set val(netif)          Phy/WirelessPhy            ;# network interface type
set val(mac)            Mac/802_11                 ;# MAC type
set val(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(x)		670			   ;# X dimension of top
set val(y)		670			   ;# Y dimension of top
set val(ifqlen)         50                         ;# max packet in ifq
set val(seed)		0.0
set val(adhocRouting)	DSR			
set val(nn)             3                          ;# number of mobilenodes
set val(cp)		"../mobility/scene/cbr-3-test"
set val(sc)		"../mobility/scene/scen-3-test"
set val(stop)		200.0			   ;#simulation time

# ======================================================================
# Main Program
# ======================================================================


#
# Initialize Global Variables
# trace file
#
set ns_		[new Simulator]

# set up topography object
set topo       [new Topography]



#create trace objects
set tracefd     [open simple802mac.tr w]
set namtrace	[open simple802mac.nam w]



$ns_ trace-all $tracefd
$ns_ namtrace-all-wireless $namtrace $val(x) $val(y)


#define topology
$topo load_flatgrid $val(x) $val(y)

#
# Create God
# God(General Operations Director) an object used to store global info
# about the state of the environment, network...should not be made known
# to any participant in simulation
# stores total # of mobile nodes, table of shortest # of hops from one node
# to another
# create-god procedure is in ~ns/tcl/mobility/com.tcl
#
set god_ [create-god $val(nn)]

#
#  Create the specified number of mobilenodes [$val(nn)] and "attach" them
#  to the channel. 
#  Here two nodes are created : node(0) and node(1)

# configure node

        $ns_ node-config -adhocRouting $val(adhocRouting) \
			 -llType $val(ll) \
			 -macType $val(mac) \
			 -ifqType $val(ifq) \
			 -ifqLen $val(ifqlen) \
			 -antType $val(ant) \
			 -propType $val(prop) \
			 -phyType $val(netif) \
			 -channelType $val(chan) \
			 -topoInstance $topo \
			 -agentTrace ON \
			 -routerTrace OFF \
			 -macTrace ON \
			 -movementTrace OFF			


# create the specified number of nodes [$val(nn)] and
# attach them to the channel
			 
	for {set i 0} {$i < $val(nn) } {incr i} {
		set node_($i) [$ns_ node]	
		$node_($i) random-motion 0		;# disable random motion
	}


#define node movement model

puts "Loading connection pattern..."
source $val(cp)


#define traffic model

puts "Loading scenario file.."
source $val(sc)


#define node initial position in nam

for {set i 0} {$i < $val(nn)} {incr i} {

	#20 defines the node size in nam, adjust it according to scenario
	#function must be called after mobility model is defined

	$ns_ initial_node_pos $node_($i) 20

}


#tell nodes when simulation ends

for {set i 0} {$i < $val(nn) } {incr i} {
	$ns_ at $val(stop).0 "$node_($i) reset";
}

$ns_ at $val(stop).0002 "puts \"NS EXITING...\"; $ns_ halt"


puts $tracefd "M 0.0 nn $val(nn) x $val(x) y $val(y) rp $val(adhocRouting)"
puts $tracefd "M 0.0 sc $val(sc) cp $val(cp) seed $val(seed)"
puts $tracefd "M 0.0 prop $val(prop) ant $val(ant)"

puts "Starting Simulation..."
$ns_ run

