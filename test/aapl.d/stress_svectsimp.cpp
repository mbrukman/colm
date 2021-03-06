/*
 * Copyright 2002, 2003 Adrian Thurston <thurston@colm.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "vectsimp.h"
#include "svectsimp.h"

using namespace std;
void processArgs( int argc, char** argv );

template class SVectSimp<char>;

/* Data structure whos constructors and destructor count callings. */
struct TheData
{
	TheData() : i(0) { }
	TheData(int i) : i(i) { }
	TheData(const TheData &d) : i(1) { }
	~TheData() { }

	int i;
};

bool operator==(const TheData &td1, const TheData &td2)
	{ return td1.i == td2.i; }

#define LEN_THRESH 1000
#define STATS_PERIOD 211
#define OUTBUFSIZE 1000
//#define NUM_ROUNDS 1e9
#define NUM_ROUNDS 100000
int curRound = 0;

void expandTab( char *dst, char *src );

/* Print a statistics header. */
void printHeader()
{
	char buf[OUTBUFSIZE];
	expandTab( buf, "round\tlength" );
	cout << buf << endl;
}


/* Replace the current stats line with new stats. For one tree. */
void printStats( VectSimp<TheData> &v1 )
{
	/* Print stats. */
	static char buf[OUTBUFSIZE] = { 0 };
	char tmpBuf[OUTBUFSIZE];
	memset( buf, '\b', strlen(buf) );
	cout << buf;
	sprintf( tmpBuf, "%i\t%li\t", curRound, v1.length() );
	expandTab( buf, tmpBuf );
	cout << buf;
	cout.flush();
}

VectSimp<TheData> v1;
SVectSimp<TheData> v2;

VectSimp<TheData> v1Copy;
SVectSimp<TheData> v2Copy;

int main( int argc, char **argv )
{
	processArgs( argc, argv );
	srandom(time(0));
	printHeader();

	for ( curRound = 0; ; curRound++ ) {
		/* Choose an action. */
		int action = random() % 7;
		/* 0: remove
		 * 1: setAs
		 * 2: prepend
		 * 3: append
		 * 4: insert
		 * 5: replace
		 * 6: copy
		 */

		/* Exclude remove when already empty. */
		if ( action == 0 && v1.length() == 0 )
			continue;

		/* Exclude growing when at max len. */
		if ( 3 <= action && v1.length() > LEN_THRESH )
			continue;

		switch ( action ) {
			/* remove. */
			case 0: {
				/* Get a position and length. */
				int pos = random() % (v1.length()+1);
				int len = random() % (v1.length() - pos + 1);
				v1.remove( pos, len );
				v2.remove( pos, len );
				break;
			}
			/* setAs. */
			case 1: {
				int len = random() % LEN_THRESH;
				TheData *tmpD = new TheData[len];
				for ( int i = 0; i < len; i++ )
					tmpD[i].i = random();
				v1.setAs( tmpD, len );
				v2.setAs( tmpD, len );
				delete[] tmpD;
				break;
			}
			/* prepend. */
			case 2: {
				int len = random() % LEN_THRESH;
				TheData *tmpD = new TheData[len];
				for ( int i = 0; i < len; i++ )
					tmpD[i].i = random();
				v1.prepend( tmpD, len );
				v2.prepend( tmpD, len );
				delete[] tmpD;
				break;
			}
			/* append. */
			case 3: {
				int len = random() % LEN_THRESH;
				TheData *tmpD = new TheData[len];
				for ( int i = 0; i < len; i++ )
					tmpD[i].i = random();
				v1.append( tmpD, len );
				v2.append( tmpD, len );
				delete[] tmpD;
				break;
			}
			/* insert. */
			case 4: {
				int pos = random() % (v1.length()+1);
				int len = random() % LEN_THRESH;
				TheData *tmpD = new TheData[len];
				for ( int i = 0; i < len; i++ )
					tmpD[i].i = random();
				v1.insert( pos, tmpD, len );
				v2.insert( pos, tmpD, len );
				delete[] tmpD;
				break;
			}
			/* replace. */
			case 5: {
				int pos = random() % (v1.length()+1);
				int len = random() % LEN_THRESH;
				TheData *tmpD = new TheData[len];
				for ( int i = 0; i < len; i++ )
					tmpD[i].i = random();
				v1.replace( pos, tmpD, len );
				v2.replace( pos, tmpD, len );
				delete[] tmpD;
				break;
			}
			/* copy. */
			case 6: {
				VectSimp<TheData> v1Loc(v1);
				v1Copy = v1;

				SVectSimp<TheData> v2Loc(v2);
				v2Copy = v2;
			}
		}

		/* Assert that the vectors are equal. */
		VectSimp<TheData>::Iter v1It = v1;
		SVectSimp<TheData>::Iter v2It = v2;
		assert( v1.size() == v2.size() );
		for ( int i = 0; i < v1.size(); i++, v1It++, v2It++ )
			assert( *v1It == *v2It );

		if ( curRound % STATS_PERIOD )
			printStats( v1 );

		curRound += 1;
	}
	cout << endl;
	return 0;
}
