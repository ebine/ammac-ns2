function average (array) {
    sum = 0;
    items = 0;
    for (i in array) {
        sum += array[i];
        items++;
    }
#    printf("DEBUG                sum is %d, items is %d\n", sum, items);
    if (sum == 0 || items == 0)
        return 0; 
    else
        return sum / items;
}

function max( array ) {
    for (i in array) {
        if (array[i] > largest)
            largest = array[i];
    }
    return largest;
}

function min(array) {
    for (i in array) {
        if (0 == smallest)
            smallest = array[i];
        else if (array[i] < smallest)
            smallest = array[i];
    }
    return smallest;
}
BEGIN {
    total_packets_sent = 0;
    total_packets_received = 0;
    total_packets_dropped = 0;
    first_packet_sent = 0;
    last_packet_sent = 0;
    last_packet_received = 0;
}
{
    event = $1;
    time = $2;
    node = $3;
    type = $4;
    reason = $5;
    packetid = $6;

# strip leading and trailing _ from node
    sub(/^_*/, "", node);
    sub(/_*$/, "", node);

    if ( time < simulation_start || simulation_start == 0 )
        simulation_start = time;
    if ( time > simulation_end )
        simulation_end = time;

    if ( reason == "COL" )
            total_collisions++;

    if ( type == "AGT" ) {
        nodes[node] = node; # to count number of nodes
        if ( time < node_start_time[node] || node_start_time[node] == 0 )
            node_start_time[node] = time;

        if ( time > node_end_time[node] )
            node_end_time[node] = time;

        if ( event == "s" ) {
            flows[node] = node; # to count number of flows
            if ( time < first_packet_sent || first_packet_sent == 0 )
                first_packet_sent = time;
            if ( time > last_packet_sent )
                last_packet_sent = time;
            # rate
            packets_sent[node]++;
            total_packets_sent++;

            # delay
            pkt_start_time[packetid] = time;
        }
        else if ( event == "r" ) {
            if ( time > last_packet_received )
                last_packet_received = time;
            # throughput
            packets_received[node]++;
            total_packets_received++;

            # delay
            pkt_end_time[packetid] = time;
        }
        else if ( event == "D" ) {
            total_packets_dropped++;
#           pkt_end_time[packetid] = time; # EXPERIMENTAL
        }
    }
}
END {
    print "" > "throughput.dat";
    print "" > "rate.dat";
    aggregatethru = 0;
    number_flows = 0;
    for (i in flows)
        number_flows++;

    # find dropped packets
    if ( total_packets_sent != total_packets_received ) {
        printf("***OUCH*** Dropped Packets!\n\n");
        for ( packetid in pkt_start_time ) {
            if ( 0 == pkt_end_time[packetid] ) {
                total_packets_dropped++;
#               pkt_end_time[packetid] = simulation_end; # EXPERIMENTAL
            }
        }
    }

    for (i in nodes) {
        if ( packets_received[i] > 0 ) {
# 	printf("node %d packets num is %d\n", i, packets_received[i]);
            end = node_end_time[i];
            start = node_start_time[i - number_flows];
            runtime = end - start;
            if ( runtime > 0 ) {
                throughput[i] = packets_received[i]*8000 / runtime;
		aggregatethru += throughput[i];
                printf("%d %f %f %d\n", i, start, end, throughput[i]) >> "throughput.dat";
            }
        }
        # rate - not very accurate
        if ( packets_sent[i] > 2 ) {
            end = node_end_time[i];
            start = node_start_time[i];
            runtime = end - start;
            if ( runtime > 0 ) {
                rate[i] = (packets_sent[i]*8000) / runtime;
                printf("%d %f %f %d\n", i, start, end, rate[i]) >> "rate.dat";
            }
        }
    }

    # delay
    for ( pkt in pkt_end_time) {
        end = pkt_end_time[pkt];
        start = pkt_start_time[pkt];
        delta = end - start;
        if ( delta > 0 ) {
            delay[pkt] = delta;
            printf("%d %f %f %f\n", pkt, start, end, delta) > "delay.dat";
        }
    }
   
    # offered load
    total_runtime = last_packet_sent - first_packet_sent;
    if ( total_runtime > 0 && total_packets_sent > 0)
        load = ((total_packets_sent * 8000)/total_runtime) / 2000000; # no overhead

    printf("\
      RUN   OFFERED PACKETS  PACKETS  PACKETS             AVERAGE    MAX        MIN        AVERAGE      AVERAGE		AGGREGATE\n\
FLOWS TIME  LOAD    SENT     RECEIVED DROPPED  COLLISIONS DELAY      DELAY      DELAY      THROUGHPUT   TRAFFIC RATE	THROUGHPUT\n\
----- ----- ------- -------- -------- -------- ---------- ---------- ---------- ---------- ------------ -------------	----------\n");

    printf("%5d %5.1f %7.4f %8d %8d %8d %10d %10.4f %10.4f %10.4f %12d %12d %12d\n",
           number_flows,
           total_runtime,
           load,
           total_packets_sent,
           total_packets_received,
           total_packets_dropped,
           total_collisions,
           average(delay),
           max(delay),
           min(delay),
           average(throughput),
           average(rate),
	   aggregatethru);

    printf("%5d %5.1f %7.4f %8d %8d %8d %10d %10.4f %10.4f %10.4f %12d %12d %12d\n",
           number_flows,
           total_runtime,
           load,
           total_packets_sent,
           total_packets_received,
           total_packets_dropped,
           total_collisions,
           average(delay),
           max(delay),
           min(delay),
           average(throughput),
	   aggregatethru, 
           average(rate)) >> "stats.dat";
}

