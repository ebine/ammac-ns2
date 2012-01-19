#!/usr/bin/python

"""Stupid Plot

A stupid Python wrapper around GnuPlot. It automates some of the tasks involved
in creating GnuPlot output from Python data.

To use it, call the gnuplotTable() function with your data (2D list) and output
file. It will do its best attempt to plot it in EPS format. To customize stuff, use
the gnuplotOptions dictionary. You will likely need to know a bit about
GnuPlot and read the source to this module to understand what is going on.
"""

# The heart of this module is the "gnuplotTable" function. Actually, that's all that is here!

import tempfile
import os

# A few "standard" label types
# 12.345
EXPONENTIAL_LABEL = "%.0t{/Symbol \264}10^{%T}" # 1*10^1
EXPONENTIAL_LABEL1 = "%.1t{/Symbol \264}10^{%T}" # 1*10^1

def histogram( data, numBuckets, minTruncate=None, maxTruncate=None ):
	'''Pass in a list of data values and the number of buckets. It will return a list of
	coordinates that will draw a nice histogram in GnuPlot with lines.'''

	data = list( data )
	data.sort()
	
	minValue = data[0]
	maxValue = data[-1]
	
	if maxTruncate != None:
		maxValue = min( maxTruncate, maxValue )
	if minTruncate != None:
		assert minTruncate < maxValue
		minValue = max( minTruncate, minValue )
	
	interval = float(maxValue-minValue)/numBuckets
	
	buckets = []
	
	bucketCount = 0
	bucketLimit = minValue + interval
	
	for value in data:
		while value > bucketLimit and len(buckets) < numBuckets-1:
			#~ print bucketLimit, bucketCount
			buckets.append( bucketCount )
			bucketLimit = minValue + interval*(len( buckets ) + 1)
			bucketCount = 0
		
		if value > maxValue:
			break
		
		bucketCount += 1
	buckets.append( bucketCount )
	
	assert abs( bucketLimit - maxValue ) < 0.00001
	
	# Draw the line for gnuplot
	total = float( len( data ) )
	
	out = []
	
	# Start at zero
	if buckets[0] != 0:
		out.append( (minValue, 0) )
	
	previous = minValue
	for i, bucketCount in enumerate( buckets ):
		next = minValue + interval*(i + 1)
		
		out.append( (previous, bucketCount/total*100.0) )
		out.append( (next, bucketCount/total*100.0) )
		
		previous = next
	
	# End at zero
	if buckets[-1] != 0:
		out.append( (previous, 0) )
	
	assert abs( out[-1][0] - maxValue ) < 0.00001
	
	return out

def hackDottedStyle( epsFile ):
	'''GNUPlot's default dotted line styles suck. This will hack the given EPS so
	the lines are more easily distinguishable.'''
	
	input = file( epsFile )
	inLines = input.readlines()
	input.close()
	
	output = file( epsFile, 'w' )
	for line in inLines:
		if line.startswith( '/LT1 {' ):
			output.write( '/LT1 { PL [5 dl 2 dl] 0 1 1 DL } def\n' )
		elif line.startswith( '/LT2 {' ):
			output.write( '/LT2 { PL [4 dl 2 dl 1 dl 2 dl] 0 1 1 DL } def\n' )
		elif line.startswith( '/LT3 {' ):
			output.write( '/LT3 { PL [1 dl 2 dl] 1 0 1 DL } def\n' )
		elif line.startswith( '/LT4 {' ):
			output.write( '/LT4 { PL [5 dl 2 dl 1 dl 2 dl 1 dl 2 dl 1 dl 2 dl] 0 1 1 DL } def\n' )
		elif line.startswith( '/LT5 {' ):
			output.write( '/LT5 { PL [6 dl 2 dl 5 dl 7 dl] 0 1 1 DL } def\n' )
		elif line.startswith( '/LT6 {' ):
			output.write( '/LT6 { PL [2 dl 2 dl 2 dl 2 dl 2 dl 6 dl] 0 1 1 DL } def\n' )
		elif line.startswith( '/LT7 {' ):
			output.write( '/LT7 { PL [8 dl 5 dl 1 dl 5 dl] 0 1 1 DL } def\n' )
		else:
			output.write( line )
	output.close()

def hackBarChartColor( epsFile, numBars, skip=0 ):
	'''Creates a legible greyscale bar chart EPS from a color one.'''
	
	input = file( epsFile )
	inLines = input.readlines()
	input.close()
	
	colors = []
	# Create even colors from black to white
	color = 0.0
	step = 1.0 / (numBars-1)
	while len( colors ) < numBars:
		colors.append( color )
		color = len( colors ) * step
	
	output = file( epsFile, 'w' )
	for line in inLines:
		if line.startswith( '/LT' ) and line[3].isdigit():
			barNumber = int( line[3] )
			if barNumber + skip < len( colors ):
				color = colors[barNumber+skip]
				output.write( '/LT%d { PL [] %f %f %f DL } def\n' % ( barNumber, color, color, color ) )
				# Skip the rest of the processing
				continue
				
		output.write( line )
	output.close()


# NOTE: If you pass "plottype": 'barchart' this script tries to plot a nice bar chart
def gnuplotTable( table, outputFile, gnuplotOptions={} ):
	"""table - 2D list of data. The first row is taken as headers.
	outputFile - The output of GnuPlot goes here (in .eps format).
	gnuplotOptions - A dict used to customize the behaviour of gnuplot."""
	
	# Make table into tables, unless it already is
	tables = table
	if not isinstance( tables[0][0], list ) and not isinstance( tables[0][0], tuple ):
		tables = ( tables, )
	
	# For the tables to work, the first column must have the same header, not necissarily the same values
	for table in tables:
		assert( table[0][0] == tables[0][0][0] )
	
	# plot formatting options
	options = {
		"gnuplot": "gnuplot",
		"title" : "",
		"xrange": "[]",
		"yrange": "[]",
		"xlabel": tables[0][0][0],
		"ylabel": "Y Axis Values",
		"grid": "",
		"xformat": r"%0.1s%c",
		"yformat": r"%0.1s%c",
		"linewidth": 4,
		"fontsize": 16,
		"color": True,
		"plottype": "lines", # Changed the default "plottype"
		"plottypes": {}, # Overrides the default "plottype"
		"key": "",
		"size": "",
		"boxstuff": "",
		"pointsize": "",
	}
	# Copy the user supplied options into our options dictionary
	for key,value in gnuplotOptions.items():
		options[key] = value
	
	# Add "set key " to the front of the key option, if it was specified
	if options["key"] != "":
		if options["key"] == False:
			options["key"] = "set nokey"
		else:
			options["key"] = "set key " + options["key"]
	# Add "set size " to the front of the size option, if it was specified
	if options["size"] != "": options["size"] = "set size " + options["size"]
	# Add "set pointsize " to the front of the size option, if it was specified
	if options["pointsize"] != "": options["pointsize"] = "set pointsize " + str( options["pointsize"] )
		

	color = "color"
	solid = "set terminal postscript solid"
	if not options["color"] and options['plottype'] != 'barchart':
		color = "monochrome"
		solid = ""

	boxWidth = None
	if options['plottype'] == 'barchart':
		options['barchart'] = True
		options['xformat'] = "%s"
		xtics = [ None ]
		
		# Number of X values = number of rows in the tables
		numXValues = len( tables[0] ) - 1
		# Number of boxes = number of columns in the tables
		numBoxes = 0
		for table in tables:
			numBoxes += (len( table[0] ) - 1)
			
			for i, row in enumerate( table ):
				#~ print i, row
				if i == 0: continue
					
				if i < len( xtics ):
					assert( row[0] == xtics[i] )
				else:
					xtics.append( row[0] )
					
				row[0] = i
		
		# This box width is sufficient to leave one empty box between clusters
		boxWidth = 1.0 / (1 + numBoxes)
		
		options['xrange'] = "[%f:%f]" % (1 - (boxWidth*numBoxes)/2, numXValues + (boxWidth*numBoxes)/2)
		options['grid'] = 'noxtics ytics linewidth 2.0'
		options['plottype'] = 'boxes'

		xticString = "( "
		for i, x in enumerate( xtics ):
			if i == 0: continue
			
			xticString += '"%s" %d, ' % ( x, i )
		xticString = xticString[:-2] + ")"
		
		#set xtics rotate %s\n
		options['boxstuff'] = 'set ticscale 0 0\nset xtics %s\nset boxwidth %f\nset style fill solid border -1' % ( xticString, boxWidth )

	
	# Gnuplot output
	scriptfile = """
set title "%s"
set xlabel "%s"
set ylabel "%s"
set grid %s

# Set the axes to engineering notation
set format x '%s'
set format y '%s'

set xrange %s
set yrange %s

set terminal postscript "Helvetica" %d
set terminal postscript %s # color or monochrome
%s # Use solid or dotted lines
set terminal postscript eps enhanced
set output "%s"

%s
%s
%s
%s
""" % ( options["title"], options["xlabel"], options["ylabel"],
	 options["grid"], options["xformat"], options["yformat"],
	options["xrange"], options["yrange"], options["fontsize"],
	color, solid, outputFile, options["key"], options["size"], options["boxstuff"],
	options["pointsize"] )

	if 'calculated' in options:
		scriptfile += 'f(x) = %s\n' % options['calculated']
	
	tempDataFiles = []
	
	plotLines = []
	for table in tables:
		data = tempfile.NamedTemporaryFile()
		# Skip the headers in the data files. On occasion they confuse GnuPlot
		for line in table[1:]:
			data.write( "\t".join( [str(i) for i in line] ) )
			data.write( "\n" )
		data.flush()
		tempDataFiles.append( data )
	
		headings = table[0]
		
		for i,heading in enumerate( headings[1:] ):
			# Skip this column if it is an error bar column
			if ("errorbars" in options) and ((i-1) in options["errorbars"]): continue
			
			plottype = options["plottype"]
			if i in options["plottypes"]: plottype = options["plottypes"][i]
			
			if boxWidth:
				offset = (-(numBoxes-1)/2.0 + i) * boxWidth
				#~ print "numBoxes =", numBoxes, "boxWidth =", boxWidth, "i =",i, "offset =", offset
				plotLines.append( ' "%s" using ($1+%f):%d title "%s" with %s' % ( data.name, offset, i+2, heading, plottype ) )
			else:
				plotLines.append( ' "%s" using 1:%d title "%s" with %s linewidth %d' % ( data.name, i+2, heading, plottype, options["linewidth"] ) )
			
				if ("errorbars" in options) and (i in options["errorbars"]):
					plotLines.append( '"%s" using 1:%d:%d notitle with yerrorbars linetype %d linewidth %d' % ( data.name, i+2, i+3, i+1, options["linewidth"]-1 ) )
	
	if 'calculated' in options:
		plotLines.insert( 0, 'f(x) with lines' )
	
	scriptfile += "plot " + ", ".join( plotLines )
	
	script = tempfile.NamedTemporaryFile()
	script.write( scriptfile )
	script.flush()
	
	#~ import shutil
	#~ shutil.copyfile( data.name, "data.txt" )
	#~ shutil.copyfile( script.name, "script.txt" )
	
	code = os.spawnlp( os.P_WAIT, options["gnuplot"], options["gnuplot"], script.name )
	assert( code == 0 )
	
	script.close()
	for data in tempDataFiles:
		data.close()
	
	if not options['color']:
		if 'barchart' in options and options['barchart']:
			hackBarChartColor( outputFile, numBoxes )
		else:
			hackDottedStyle( outputFile )
	

