#!/usr/bin/python

"""A floating-point value of xrange. Returns a generator which creates a sequence of floating-point values"""

def frange(start, end=None, increment=1.0):
	"""frange([start,] stop[, step]) -> generator
	
	Returns an object that generates a sequence of floating-point
	values. It has the same interface as range() and xrange().
	frange(i, j) returns [i, i+1, i+2, ..., j-1]; start (!) defaults to 0.
	When step is given, it specifies the increment (or decrement).
	The arguments can be integer or floating-point values.
	"""
	
	if end == None:
		end = start
		start = 0.0

	if increment == 0.0:
		raise ValueError, "frange() step must not be zero"
	if increment < 0 and end > start:
		return
	increment = float( increment )
	
	lowerBound = min( start, end )
	upperBound = max( start, end )

	index = 0
	next = float( start )
	while (lowerBound < next or increment > 0 and index == 0) and (next < upperBound or increment < 0 and index == 0):
		last = next
		index += 1
		next = start + index * increment
		yield last
	
	return

if __name__ == "__main__":
	# Compare the output of frange to range
	def checkLists( *args ):
		aFloat = [ float( i ) for i in range( *args ) ]
		bList = list( frange( *args ) )
		#~ print aFloat
		#~ print bList
		assert( aFloat == bList )

	checkLists( 0 )
	checkLists( 5 )
	checkLists( 2, 5 )
	checkLists( 5, 5 )
	checkLists( 5, 2 )
	checkLists( 2, 5, 2 )
	checkLists( 2, 5, -1 )
	checkLists( 5, 2, -2 )
	checkLists( 5, 5, -2 )
