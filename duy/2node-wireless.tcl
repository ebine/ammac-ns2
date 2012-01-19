# 2node-wireless.tcl
# A two node wireless simulation with no movement
# Ignore the time, average the bytes/second, total the lost packets
### columnSelect[1] = COLUMN_SELECT_NONE
### columnSelect[2] = COLUMN_SELECT_AVERAGE
### columnTitles[0] = "Packet Interval"
### columnTitles[2] = "Throughput"
### columnTitles[3] = "Packet Loss"

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
set val(ifqlen)         50                         ;# max packet in ifq
set val(nn)             2                          ;# number of mobilenodes
set val(rp)             DSDV                       ;# routing protocol

# ======================================================================
# Main Program
# ======================================================================

#
# Initialize Global Variables
#
set ns_		[new Simulator]
set tracefd     [open 2nodetrace.tr w]
$ns_ trace-all $tracefd
$ns_ use-newtrace

# Disable DSDV periodic updates
# http://www-ece.rice.edu/~jpr/ns-802_11b.html
Agent/DSDV set perup_       15000000   ;# ~ infinite periodic update
# Up the data rate to 802.11b rates
Mac/802_11 set dataRate_ 2Mb
# Disable RTS
Mac/802_11 set RTSThreshold_ 3000
# Switch to the short preamble
#Mac/802_11 set PreambleLength_ 72


# set up topography object
set topo       [new Topography]

$topo load_flatgrid 500 500

#
# Create God
#
create-god $val(nn)

#
#  Create the specified number of mobilenodes [$val(nn)] and "attach" them
#  to the channel. 
#  Here two nodes are created : node(0) and node(1)

# Create a new channel
set channel1_ [new $val(chan)]

# configure node

        $ns_ node-config -adhocRouting $val(rp) \
			 -llType $val(ll) \
			 -macType $val(mac) \
			 -ifqType $val(ifq) \
			 -ifqLen $val(ifqlen) \
			 -antType $val(ant) \
			 -propType $val(prop) \
			 -phyType $val(netif) \
			 -channel $channel1_ \
			 -topoInstance $topo \
			 -agentTrace ON \
			 -routerTrace ON \
			 -macTrace OFF \
			 -movementTrace OFF			
			 
	for {set i 0} {$i < $val(nn) } {incr i} {
		set node_($i) [$ns_ node]	
		$node_($i) random-motion 0		;# disable random motion
	}

#
# Provide initial (X,Y, for now Z=0) co-ordinates for mobilenodes
#
$node_(0) set X_ 0.0
$node_(0) set Y_ 0.0
$node_(0) set Z_ 0.0

$node_(1) set X_ 1.0
$node_(1) set Y_ 0.0
$node_(1) set Z_ 0.0

# Setup traffic flow between nodes
# UDP connections between node_(0) and node_(1)
set udp [new Agent/UDP]
set cbr [new Application/Traffic/CBR]
$udp set packetSize_ 1500
# 1500 - 20 byte IP header = 1480 bytes
$cbr set packetSize_ 1480 ;# This size INCLUDES the UDP header
##### frange( 0.001, 0.05, 0.0005 ) 
$cbr set interval_ 0.05000
$cbr attach-agent $udp

set sink [new Agent/LossMonitor]
$ns_ attach-agent $node_(0) $udp
$ns_ attach-agent $node_(1) $sink
$ns_ connect $udp $sink

# Open a sink statistics file
set f [open 2nodestats.tr w]

proc record {} {
	global sink f
	
	#Get an instance of the simulator
	set ns [Simulator instance]
	
	#Set the time after which the procedure should be called again
	set time 0.5
	
	#How many bytes have been received by the traffic sinks?
	set bw [$sink set bytes_]
	set drop [$sink set nlost_]

	#Get the current time
	set now [$ns now]
	#Calculate the bandwidth (in bytes/s) and write it to the files
	puts $f "$now [expr $bw/$time] $drop"
	
	#Reset the bytes_ values on the traffic sinks
	$sink set bytes_ 0
	$sink set nlost_ 0

	#Re-schedule the procedure
	$ns at [expr $now+$time] "record"
}

# Start the CBR agent after routing has stabilized
$ns_ at 30.0 "record"
$ns_ at 30.0 "$cbr start"

$ns_ at 150.0000001 "stop"
$ns_ at 150.0000002 "puts \"NS EXITING...\" ; $ns_ halt"

proc stop {} {
    global ns_ tracefd f
    $ns_ flush-trace
    close $tracefd
    close $f
}

puts "Starting Simulation..."
$ns_ run
