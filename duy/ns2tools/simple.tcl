# ====================================================================== 
# Define options 
# ====================================================================== 
set val(chan)         Channel/WirelessChannel  ;# channel type 
set val(prop)         Propagation/TwoRayGround ;# radio-propagation model 
set val(ant)          Antenna/OmniAntenna      ;# Antenna type 
set val(ll)           LL                       ;# Link layer type 
set val(ifq)          CMUPriQueue  ;# Interface queue type 
set val(ifqlen)       50                       ;# max packet in ifq 
set val(netif)        Phy/WirelessPhy          ;# network interface type 
set val(mac)          Mac/802_11               ;# MAC type 
set val(rp)           DSR                     ;# ad-hoc routing protocol  
set val(nn)           5                        ;# number of mobilenodes 


# Create simulator
set ns_    [new Simulator]

# Set up trace file
$ns_ use-newtrace
set tracefd [open simple.tr w]
$ns_ trace-all $tracefd

# Create the "general operations director"
# Used internally by MAC layer: must create!
create-god $val(nn)


# Create and configure topography (used for mobile scenarios)
set topo [new Topography]
# 1000x1000m terrain
$topo load_flatgrid 1000 1000

$ns_ node-config -adhocRouting $val(rp) \
	-llType $val(ll) \
	-macType $val(mac) \
	-ifqType $val(ifq) \
	-ifqLen $val(ifqlen) \
	-antType $val(ant) \
	-propType $val(prop) \
	-phyType $val(netif) \
	-channel [new $val(chan)] \
	-topoInstance $topo \
	-agentTrace ON \
	-routerTrace ON \
	-macTrace OFF \
	-movementTrace OFF

for {set i 0} {$i < $val(nn) } {incr i} {
	set node_($i) [$ns_ node]	
	$node_($i) random-motion 0		;# disable random motion
	$node_($i) set Y_ 0.0
	$node_($i) set Z_ 0.0
}

$node_(0) set X_ 0.0
$node_(1) set X_ 200.0
$node_(2) set X_ 400.0
$node_(3) set X_ 600.0
$node_(4) set X_ 800.0


# 1500 - 20 byte IP header - 40 byte TCP header = 1440 bytes
Agent/TCP set packetSize_ 1440 ;# This size EXCLUDES the TCP header

set agent [new Agent/TCP]
set app [new Application/FTP]
set sink [new Agent/TCPSink]

$app attach-agent $agent

$ns_ attach-agent $node_(0) $agent
$ns_ attach-agent $node_(4) $sink
$ns_ connect $agent $sink

# 120 seconds of running the simulation time
$ns_ at 0.0 "$app start"
$ns_ at 120.0 "$ns_ halt"
$ns_ run

$ns_ flush-trace
close $tracefd
