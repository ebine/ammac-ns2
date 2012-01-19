# mmac-36.tcl
# common network environment & topology file
# 36 nodes in 6*6 grid form, WLAN (single hop)

# ======================================================================
# Define options
# ======================================================================
set opt(chan)           Channel/WirelessChannel    ;# channel type
set opt(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set opt(netif)          Phy/WirelessPhy            ;# network interface type
set opt(mac)            Mac/802_11                 ;# MAC type
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set opt(ll)             LL                         ;# link layer type
set opt(ant)            Antenna/OmniAntenna        ;# antenna model
set opt(ifqlen)         50                         ;# max packet in ifq
set opt(nn)             121                       ;# number of mobilenodes

#set opt(flow)		80
set opt(row)		11	
set opt(column)		11	

set opt(adhocRouting)   DSDV                       ;# routing protocol
#set opt(adhocRouting)   DumbAgent	;# routing protocol

set opt(cp)             "tcl/mobility/scene/cbr-mh"                         ;# connection pattern file
#set opt(sc)     	"tcl/mobility/scene/scen-64k"    		     ;# node movement file. 
set opt(sc)             "tcl/mobility/scene/mh3"
set opt(sc)     	""    			;# node movement file. 

set opt(x)      1000                            ;# x coordinate of topology
set opt(y)      1000                            ;# y coordinate of topology
set opt(seed)   0	                           ;# seed for random number gen.
set opt(stop)   40 	;# time to stop simulation

Mac/802_11 set bandwidth_ 54.0e6
Mac/802_11 set basicRate_ 2.0e6
Mac/802_11 set dataRate_ 2.0e6


# ============================================================================
# check for boundary parameters and random seed
if { $opt(x) == 0 || $opt(y) == 0 } {
	puts "No X-Y boundary values given for wireless topology\n"
}
if {$opt(seed) > 0} {
	puts "Seeding Random number generator with $opt(seed)\n"
	ns-random $opt(seed)
}

# create simulator instance
set ns_   [new Simulator]

set tracefd  [open origmac.tr w]
$ns_ trace-all $tracefd

# Create topography object
set topo   [new Topography]

# define topology
$topo load_flatgrid $opt(x) $opt(y)

# create God
create-god [expr $opt(nn)]

# configure for base-station node
$ns_ node-config -adhocRouting $opt(adhocRouting) \
 	  	 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop) \
                 -phyType $opt(netif) \
                 -channelType $opt(chan) \
				 -topoInstance $topo \
				 -agentTrace ON \
                 -routerTrace OFF \
                 -macTrace ON \
				 -movementTrace OFF

	 
# create mobilenodes in the same domain as BS(0)  
# note the position and movement of mobilenodes is as defined
# in $opt(sc)

#configure for mobilenodes

for {set j 0} {$j < $opt(nn)} {incr j} {
	set node_($j) [$ns_ node]
}


#for {set j 0} {$j < $opt(row)} {incr j} {
#	for {set i 0} {$i < $opt(column)} {incr i} {
#		$node_([expr $j * $opt(column) + $i]) set X_ [expr $i * 10]
#		$node_([expr $j * $opt(column) + $i]) set Y_ [expr $j * 10]
#		$node_([expr $j * $opt(column) + $i]) set Z_ 0
#	}
#}
 		
#mmac
#for {set j 0} {$j < $opt(nn)} {incr j} {
#	[$node_($j) set mac_(0)] make-pc
#	[$node_($j) set mac_(0)] beaconperiod 100 
#	[$node_($j) set mac_(0)] atimwindowsize 20
#}


#load traffic file
#source $opt(traffic)

###########################################################


if {$argc == 1} {
	set inter [lindex $argv 0]
} else {
	set inter 1 
}

#set pktsize 512 

#puts $rate
#puts "\n"

#set udp_(1) [new Agent/UDP]
#$ns_ attach-agent $node_(0) $udp_(1)
#set null_(1) [new Agent/Null]
#$ns_ attach-agent $node_(1) $null_(1)
#set cbr_(1) [new Application/Traffic/CBR]
#$cbr_(1) set packetSize_ $pktsize 
#$cbr_(1) set interval_ 0.025 
#$cbr_(1) set random_ 1
#$cbr_(1) set rate_ $rate
#$cbr_(1) attach-agent $udp_(1)
#$ns_ connect $udp_(1) $null_(1)
#$ns_ at 9.8 "$cbr_(1) start"
#$ns_ at 30.0 "$cbr_(1) stop"
#
#set udp_(2) [new Agent/UDP]
#$ns_ attach-agent $node_(2) $udp_(2)
#set null_(2) [new Agent/Null]
#$ns_ attach-agent $node_(3) $null_(2)
#set cbr_(2) [new Application/Traffic/CBR]
#$cbr_(2) set packetSize_ $pktsize 
##$cbr_(2) set interval_ 0.025 
#$cbr_(2) set random_ 1
#$cbr_(1) set rate_ $rate
#$cbr_(2) attach-agent $udp_(2)
#$ns_ connect $udp_(2) $null_(2)
#$ns_ at 9.9 "$cbr_(2) start"
#$ns_ at 30.0 "$cbr_(2) stop"
#
#set udp_(3) [new Agent/UDP]
#$ns_ attach-agent $node_(4) $udp_(3)
#set null_(3) [new Agent/Null]
#$ns_ attach-agent $node_(5) $null_(3)
#set cbr_(3) [new Application/Traffic/CBR]
#$cbr_(3) set packetSize_ $pktsize 
##$cbr_(3) set interval_ 0.025 
#$cbr_(3) set random_ 1
#$cbr_(1) set rate_ $rate
##$cbr_(3) attach-agent $udp_(3)
#$ns_ connect $udp_(3) $null_(3)
#$ns_ at 10.0 "$cbr_(3) start"
#$ns_ at 30.0 "$cbr_(3) stop"


###########################################################

# source connection-pattern and node-movement scripts
if { $opt(cp) == "" } {
	puts "*** NOTE: no connection pattern specified."
        set opt(cp) "none"
} else {
	puts "Loading connection pattern..."
	source $opt(cp)
}
if { $opt(sc) == "" } {
	puts "*** NOTE: no scenario file specified."
        set opt(sc) "none"
} else {
	puts "Loading scenario file..."
	source $opt(sc)
	puts "Load complete..."
}

puts " "
puts "DUYMAC  Flow:44  Rate:0.025  P-Size:512  A-Size:20"
puts " "

# Tell all nodes when the simulation ends
for {set i 0} {$i < $opt(nn) } {incr i} {
    $ns_ at $opt(stop).0 "$node_($i) reset";
}

$ns_ at $opt(stop).0004 "stop"
$ns_ at $opt(stop).0005 "puts \"NS EXITING...\" ; $ns_ halt"

proc stop {} {
    global ns_ tracefd
    $ns_ flush-trace
    close $tracefd
}

# informative headers for CMUTracefile
#puts $tracefd "M 0.0 nn $opt(nn) x $opt(x) y $opt(y) rp \
#	$opt(adhocRouting)"
#puts $tracefd "M 0.0 sc $opt(sc) cp $opt(cp) seed $opt(seed)"
#puts $tracefd "M 0.0 prop $opt(prop) ant $opt(ant)"

puts "Starting Simulation..."
$ns_ run

