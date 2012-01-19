
if {$argc !=2} {
        puts "Usage: ns test.tcl  RTSThreshold PacketSize "
        puts "Example:ns test.tcl 3000 144"
        exit
}
 
set par1 [lindex $argv 0]
set par2 [lindex $argv 1]
 
set val(chan)           Channel/WirelessChannel    ;# channel type
set val(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set val(netif)          Phy/WirelessPhy            ;# network interface type
set val(mac)            Mac/802_11                 ;# MAC type
set val(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(ifqlen)         50                         ;# max packet in ifq
 
Mac/802_11 set RTSThreshold_  $par1                ;# bytes
Mac/802_11 set ShortRetryLimit_       7               ;# retransmittions
Mac/802_11 set LongRetryLimit_        4               ;# retransmissions
Mac/802_11 set PreambleLength_        72             ;# 72 bit
Mac/802_11 set dataRate_ 11Mb
 
set val(rp) DumbAgent
set ns [new Simulator]
 
set f [open test.tr w]
$ns trace-all $f
$ns eventtrace-all
set nf [open test.nam w]
$ns namtrace-all-wireless $nf 500 500
 
# set up topography object
set topo       [new Topography]
 
$topo load_flatgrid 500 500
 
# Create God
create-god 2
 
# create channel 
set chan [new $val(chan)]
 
$ns node-config -adhocRouting $val(rp) \
                -llType $val(ll) \
                -macType $val(mac) \
                -ifqType $val(ifq) \
                -ifqLen $val(ifqlen) \
                -antType $val(ant) \
                -propType $val(prop) \
                -phyType $val(netif) \
                        -channel $chan \
                -topoInstance $topo \
                -agentTrace ON \
                -routerTrace OFF \
                -macTrace ON \
                -movementTrace OFF 
 
for {set i 0} {$i < 2} {incr i} {
        set node_($i) [$ns node]
        $node_($i) random-motion 0
}
 
$node_(0) set X_ 30.0
$node_(0) set Y_ 30.0
$node_(0) set Z_ 0.0
 
$node_(1) set X_ 200.0
$node_(1) set Y_ 30.0
$node_(1) set Z_ 0.0
 
set udp [new Agent/UDP]
#set the sender trace file name to sd
$udp set_filename sd
$ns attach-agent $node_(0) $udp
 
set null [new Agent/UdpSink]
#set the receiver filename to rd
$null set_filename rd
$ns attach-agent $node_(1) $null
$ns connect $udp $null
 
set cbr [new Application/Traffic/CBR]
$cbr attach-agent $udp
$cbr set type_ CBR
$cbr set packet_size_ $par2
$cbr set rate_ 10Mb
$cbr set random_ false
 
$ns at 0.0 "$cbr start"
$ns at 15.0 "$cbr stop"
 
 
for {set i 0} {$i < 2} {incr i} {
        $ns initial_node_pos $node_($i) 30
        $ns at 20.0 "$node_($i) reset";
}
 
$ns at 20.0 "finish"
$ns at 20.1 "puts \"NS EXITING...\"; $ns halt"
 
#INSERT ANNOTATIONS HERE
proc finish {} {
        global ns f nf val
        $ns flush-trace
        close $f
        close $nf
 
}
 
puts "Starting Simulation..."
$ns run
