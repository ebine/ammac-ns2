#Copyright (c) 1997 Regents of the University of California.
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
# $Header: /cvsroot/nsnam/ns-2/tcl/ex/wireless-simple-mac.tcl,v 1.2 2003/07/07 18:21:15 xuanc Exp $
#
# Use simple mac rather than 802.11
#   
#     --Xuan Chen, USC/ISI, July 3, 2003
#
set val(chan)           Channel/WirelessChannel    ;#Channel Type
set val(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set val(netif)          Phy/WirelessPhy            ;# network interface type
set val(mac)            Mac/802_11                 ;# MAC type
set val(ifq)            Queue/DropTail	      	   ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(ifqlen)         50                         ;# max packet in ifq
set val(nn)             5                          ;# number of mobilenodes

# routing protocol
# routing is disable to measure MAC layer performance
set val(rp)              DumbAgent  
#set val(rp)             DSDV                     
#set val(rp)             DSR                      
#set val(rp)             AODV                     

set val(x)		350
set val(y)		350

# Initialize Global Variables
set ns_		[new Simulator]
set tracefd     [open wireless-802_11-mac.tr w]
$ns_ trace-all $tracefd

set namtrace [open wireless-802_11-mac.nam w]
$ns_ namtrace-all-wireless $namtrace $val(x) $val(y)

# set up topography object
set topo       [new Topography]

$topo load_flatgrid $val(x) $val(y)

# Create God
create-god $val(nn)

# Create channel
set chan_ [new $val(chan)]

# Create node(0) and node(1)

# configure node, please note the change below.
$ns_ node-config -adhocRouting $val(rp) \
		-llType $val(ll) \
		-macType $val(mac) \
		-ifqType $val(ifq) \
		-ifqLen $val(ifqlen) \
		-antType $val(ant) \
		-propType $val(prop) \
		-phyType $val(netif) \
		-topoInstance $topo \
		-agentTrace ON \
		-routerTrace ON \
		-macTrace ON \
		-movementTrace ON \
		-channel $chan_

for {set i 0} {$i < $val(nn)} {incr i} {
    set node_($i) [$ns_ node]
    $node_($i) random-motion 0
    $ns_ initial_node_pos $node_($i) 20
}



set god_ [God instance]

#
# Provide initial (X,Y, for now Z=0) co-ordinates for mobilenodes
#

$node_(0) set X_ 317.749661799038
$node_(0) set Y_ 217.934867548862
$node_(0) set Z_ 0.000000000000

$node_(1) set X_ 20.644559581780
$node_(1) set Y_ 132.597481559561
$node_(1) set Z_ 0.000000000000

$node_(2) set X_ 328.971809934856
$node_(2) set Y_ 22.274456960868
$node_(2) set Z_ 0.000000000000

$node_(3) set X_ 64.924929850825
$node_(3) set Y_ 176.877209355696
$node_(3) set Z_ 0.000000000000

$node_(4) set X_ 159.212714972963
$node_(4) set Y_ 248.033788972774
$node_(4) set Z_ 0.000000000000




# nodes: 5, pause: 2.00, max speed: 10.00, max x: 350.00, max y: 350.00
#
$node_(0) set X_ 253.325521815966
$node_(0) set Y_ 43.629509274601
$node_(0) set Z_ 0.000000000000
$node_(1) set X_ 43.285585963959
$node_(1) set Y_ 216.343190625632
$node_(1) set Z_ 0.000000000000
$node_(2) set X_ 301.258391482231
$node_(2) set Y_ 254.488730263816
$node_(2) set Z_ 0.000000000000
$node_(3) set X_ 2.952787399800
$node_(3) set Y_ 227.858075626306
$node_(3) set Z_ 0.000000000000
$node_(4) set X_ 36.095717923695
$node_(4) set Y_ 157.865140188008
$node_(4) set Z_ 0.000000000000
$god_ set-dist 0 1 2
$god_ set-dist 0 2 1
$god_ set-dist 0 3 2
$god_ set-dist 0 4 1
$god_ set-dist 1 2 3
$god_ set-dist 1 3 1
$god_ set-dist 1 4 1
$god_ set-dist 2 3 3
$god_ set-dist 2 4 2
$god_ set-dist 3 4 1
$ns_ at 2.000000000000 "$node_(0) setdest 235.940104496093 176.638230539568 4.305694165077"
$ns_ at 2.000000000000 "$node_(1) setdest 289.903225621644 61.877629747742 7.408393931330"
$ns_ at 2.000000000000 "$node_(2) setdest 67.367076376069 316.769917993374 2.312470551346"
$ns_ at 2.000000000000 "$node_(3) setdest 304.981715857563 345.907481549949 1.480344247052"
$ns_ at 2.000000000000 "$node_(4) setdest 162.841551418193 253.018633841508 8.517089481827"
$ns_ at 3.405706838025 "$god_ set-dist 1 2 1"
$ns_ at 3.405706838025 "$god_ set-dist 2 3 2"
$ns_ at 4.094061763827 "$god_ set-dist 0 1 1"
$ns_ at 5.209769223456 "$god_ set-dist 2 4 1"
$ns_ at 15.785499803603 "$god_ set-dist 2 3 1"
$ns_ at 18.644731053189 "$god_ set-dist 0 3 1"
$ns_ at 20.608326378558 "$node_(4) setdest 162.841551418193 253.018633841508 0.000000000000"
$ns_ at 22.608326378558 "$node_(4) setdest 251.205820171351 333.812167910610 0.336000923973"
$ns_ at 33.154122451833 "$node_(0) setdest 235.940104496093 176.638230539568 0.000000000000"
$ns_ at 33.921738987022 "$god_ set-dist 1 3 2"
$ns_ at 35.154122451833 "$node_(0) setdest 51.190362718793 97.659842486784 9.399325622958"
$ns_ at 41.279506539935 "$node_(1) setdest 289.903225621644 61.877629747742 0.000000000000"
$ns_ at 43.279506539935 "$node_(1) setdest 5.928697386563 311.468985335796 7.916478038912"
$ns_ at 48.754639432082 "$god_ set-dist 1 3 1"
$ns_ at 56.530443519109 "$node_(0) setdest 51.190362718793 97.659842486784 0.000000000000"
$ns_ at 58.530443519109 "$node_(0) setdest 227.538397600406 65.802526086784 4.268353413282"
$ns_ at 86.388167355278 "$god_ set-dist 0 1 2"
$ns_ at 91.036930569757 "$node_(1) setdest 5.928697386563 311.468985335796 0.000000000000"
$ns_ at 91.164908849284 "$god_ set-dist 0 2 2"
$ns_ at 93.036930569757 "$node_(1) setdest 35.030682498212 104.189318295423 6.150079520702"
$ns_ at 100.514420113862 "$node_(0) setdest 227.538397600406 65.802526086784 0.000000000000"
$ns_ at 102.514420113862 "$node_(0) setdest 223.909730329929 216.688794030638 7.150469654542"
$ns_ at 106.407481114302 "$god_ set-dist 0 1 1"
$ns_ at 106.667932768754 "$node_(2) setdest 67.367076376069 316.769917993374 0.000000000000"
$ns_ at 108.667932768754 "$node_(2) setdest 180.213453419134 254.313316661206 5.034181766005"
$ns_ at 109.734550209066 "$god_ set-dist 0 2 1"
$ns_ at 123.622109801294 "$node_(0) setdest 223.909730329929 216.688794030638 0.000000000000"
$ns_ at 125.622109801294 "$node_(0) setdest 221.790842859190 51.449632726499 5.988786766694"
$ns_ at 127.071067914513 "$node_(1) setdest 35.030682498212 104.189318295424 0.000000000000"
$ns_ at 129.071067914513 "$node_(1) setdest 145.502180597850 124.697840920917 2.070413001498"
$ns_ at 134.288233994951 "$node_(2) setdest 180.213453419134 254.313316661206 0.000000000000"
$ns_ at 136.288233994951 "$node_(2) setdest 24.746955686101 50.288958803309 6.329810083052"
$ns_ at 151.960283598484 "$god_ set-dist 0 3 2"
$ns_ at 153.215803247950 "$node_(0) setdest 221.790842859190 51.449632726499 0.000000000000"
$ns_ at 155.215803247950 "$node_(0) setdest 22.998399737679 316.329215314816 5.887706457787"
$ns_ at 157.372649473708 "$god_ set-dist 0 3 1"
$ns_ at 163.769784821773 "$god_ set-dist 2 3 2"
$ns_ at 169.910132846181 "$god_ set-dist 2 4 2"
$ns_ at 176.811860951886 "$node_(2) setdest 24.746955686101 50.288958803309 0.000000000000"
$ns_ at 178.811860951886 "$node_(2) setdest 337.952623445109 60.635041425025 4.598638088563"
$ns_ at 183.339964456285 "$node_(1) setdest 145.502180597851 124.697840920917 0.000000000000"
$ns_ at 185.339964456285 "$node_(1) setdest 97.638847215306 342.503616316419 5.525802089685"


#
# Destination Unreachables: 0
#
# Route Changes: 16
#
# Link Changes: 15
#
# Node | Route Changes | Link Changes
#    0 |             8 |            8
#    1 |             6 |            6
#    2 |             8 |            7
#    3 |             8 |            7
#    4 |             2 |            2
#
# nodes: 5, max conn: 4, send rate: 0.25, seed: 1
#
#
# 1 connecting to 2 at time 2.5568388786897245
#
set udp_(0) [new Agent/UDP]
$ns_ attach-agent $node_(1) $udp_(0)
set null_(0) [new Agent/Null]
$ns_ attach-agent $node_(2) $null_(0)
set cbr_(0) [new Application/Traffic/CBR]
$cbr_(0) set packetSize_ 512
$cbr_(0) set interval_ 0.25
$cbr_(0) set random_ 1
$cbr_(0) set maxpkts_ 10000
$cbr_(0) attach-agent $udp_(0)
$ns_ connect $udp_(0) $null_(0)
$ns_ at 2.5568388786897245 "$cbr_(0) start"
#
# 4 connecting to 3 at time 56.333118917575632
#
set udp_(1) [new Agent/UDP]
$ns_ attach-agent $node_(4) $udp_(1)
set null_(1) [new Agent/Null]
$ns_ attach-agent $node_(3) $null_(1)
set cbr_(1) [new Application/Traffic/CBR]
$cbr_(1) set packetSize_ 512
$cbr_(1) set interval_ 0.25
$cbr_(1) set random_ 1
$cbr_(1) set maxpkts_ 10000
$cbr_(1) attach-agent $udp_(1)
$ns_ connect $udp_(1) $null_(1)
$ns_ at 56.333118917575632 "$cbr_(1) start"
#
# 4 connecting to 0 at time 146.96568928983328
#
set udp_(2) [new Agent/UDP]
$ns_ attach-agent $node_(4) $udp_(2)
set null_(2) [new Agent/Null]
$ns_ attach-agent $node_(0) $null_(2)
set cbr_(2) [new Application/Traffic/CBR]
$cbr_(2) set packetSize_ 512
$cbr_(2) set interval_ 0.25
$cbr_(2) set random_ 1
$cbr_(2) set maxpkts_ 10000
$cbr_(2) attach-agent $udp_(2)
$ns_ connect $udp_(2) $null_(2)
$ns_ at 146.96568928983328 "$cbr_(2) start"
#
#Total sources/connections: 2/3
#




#
# Now produce some simple node movements
# Node_(1) starts to move towards node_(0)
#
#$ns_ at 0.0 "$node_(0) setdest 50.0 50.0 5.0"
#$ns_ at 0.0 "$node_(1) setdest 60.0 40.0 10.0"


# Node_(1) then starts to move away from node_(0)
#$ns_ at 3.0 "$node_(1) setdest 240.0 240.0 30.0" 

# Setup traffic flow between nodes
# TCP connections between node_(0) and node_(1)

#set tcp [new Agent/TCP]
#$tcp set class_ 2
#set sink [new Agent/TCPSink]
#$ns_ attach-agent $node_(0) $tcp
#$ns_ attach-agent $node_(1) $sink
#$ns_ connect $tcp $sink
#set ftp [new Application/FTP]
##$ftp attach-agent $tcp
#$ns_ at 0.5 "$ftp start" 

#
# Tell nodes when the simulation ends
#
for {set i 0} {$i < $val(nn) } {incr i} {
    $ns_ at 6.0 "$node_($i) reset";
}
$ns_ at 186.0 "stop"
$ns_ at 186.01 "puts \"NS EXITING...\" ; $ns_ halt"
proc stop {} {
    global ns_ tracefd
    $ns_ flush-trace
    close $tracefd
}

puts "Starting Simulation..."
$ns_ run
