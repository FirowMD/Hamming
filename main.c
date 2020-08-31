/*
	hamcode [ file ] [ block_size ]
*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // CHAR_BIT

// #region Defines

// Program extension ( which files have been encoded )
#define EXTENSION_NAME ".hcode"

// Default decode extension
#define EXTENSION_TEXT ".txt"

// Maximum block size in bytes
#define BUFFER_SIZE 10 // Supports less, than 80 bit per block

// Count of ctrl bits
#define CTRL_BIT 10

// Exceptions
#define printerr( ... )																										\
	printf( "[ Hamcode ] error: " );																						\
	printf( __VA_ARGS__ );																									\
	printf( "\n" )

// Returns value of bit number <N> in <NUM>
// $ N starts form 0 and ends at <n - 1>
#define GET_BIT( NUM, N )																									\
	( ( NUM & ( 1 << N ) ) && 1 )

// #endregion

// #region Function prototypes

// Changes extension of file
// $ Returns 0 if error occured
// $ Returns 1 if file with extension EXTENSION_NAME
// $ Returns 2 if file with other extension
unsigned char ChangeExtension( char **fname );

// Is the number a power of 2
// $ Return 1 if number is a power of 2
// $ Return 0 if not
extern inline int isPowOf2( int value );

// Write global buffer to file
// $ dest - destination file
void Write( FILE *dest );

// Pushes hamming code to file [dest]
// $ dest - output stream
// $ bit - bit to push to output file
// $ block - how many bits need to collect to push info to output file
// $ write - write down existing bits? ( 0 - false, !0 - true )
void Push( FILE *dest, unsigned char bit, char write );

// Returns next bit from [source] file
// $ Returns -1, if end of file was found
// $ Returns -2, if error occurred
char Pop( FILE *source );

// Encodes part of file [fin] and writes to [fout] with block size [block] ( in bits )
// $ Returns number of bytes, which have been read
// $ Returns 0, if error occured
void Encode( FILE *fin, FILE *fout, short block );

// Decodes file [fin] and writes to [fout] with block size [block] ( in bits )
void Decode( FILE *fin, FILE *fout, short block );

// #endregion

// #region Global variables
char push_buffer[ BUFFER_SIZE ] = { 0 };
char pop_buffer = 0;
short push_pos = 0;
short pop_pos = 0;
// #endregion

int main( int argc, char *argv[] ){
	register int i;
	unsigned char block_size = 8; // Default block size
	unsigned char encode = 0; // Encode or decode file
	char *fname = NULL;

	FILE *fin; // Input file ( which is a command prompt argument )
	FILE *fout; // Output file ( which with another extension )

	if( argc < 2 ){
		printerr( "wrong count of arguments" );
		printf( "\n" );
		printf( "Program syntax:\n" );
		printf( "> hamcode [ file ] [ block size ]\n" );
		printf( "=======================================================\n" );
		printf( "If file have an extension *.hcode, then activates a decode mode\n" );
		printf( "Else activates encode mode\n" );
	} else if( argc == 3 ){
		sscanf( argv[ 2 ], "%hhd", &block_size );
	} else if( argc != 2 ){
		sscanf( argv[ 2 ], "%hhd", &block_size );
		printerr( "too many arguments; arguments after <%s> have been skipped", argv[2] );
	}

	fname = ( char * ) calloc ( strlen( argv[ 1 ] ) + 1, sizeof( char ) );
	if( fname == NULL ){
		printerr( "out of memory" );
		exit( -1 );
	}
	strcpy( fname, argv[ 1 ] );

	fin = fopen( fname, "rb" );
	if( fin == NULL ){
		free( fname );
		printerr( "file \"%s\" has been not opened", fname );
		exit( -1 );
	}

	// Which program must be started
	encode = ChangeExtension( &fname );

	fout = fopen( fname, "wb" );
	if( fout == NULL ){
		free( fname );
		fclose( fin );
		printerr( "file \"%s\" has been not opened", fname );
		exit( -1 );
	}

	printf( "Input file: %s\n", argv[ 1 ] );
	printf( "Output file: %s\n", fname );
	printf( "Block size: %d\n", block_size );

	if( encode == 2 ){
		Encode( fin, fout, block_size );
	} else if( encode == 1 ){
		Decode( fin, fout, block_size );
	} else {
		printerr( "error occured: [ v ] encode: [ f ] ChangeExtension" );
	}

	printf( "Program completed\n" );

	free( fname );
	fclose( fin );
	fclose( fout );
	return 1;
}

// #region Function definitions

// Changes extension of file
unsigned char ChangeExtension( char **fname ){
	if( fname == NULL ){
		printerr( "[ ChangeExtension ] bad argument #1" );
		return 0;
	}

	size_t i;
	char *temp_fname = NULL;

	for( i = strlen( *fname ); i >= 0; i-- ){
		if( ( *fname )[ i ] == '.' ){
			if( strcmp( ( *fname + i ), EXTENSION_NAME ) == 0 ){
				// Convert EXTENSION_NAME to EXTENSION_TEXT
				( *fname )[ i ] = 0;
				temp_fname = ( char * ) calloc ( strlen( *fname ) + strlen( EXTENSION_TEXT ) + 1, sizeof( char ) );
				if( temp_fname == NULL ){
					printerr( "[ ChangeExtension ] out of memory" );
					return 0;
				}
				strcpy( temp_fname, *fname );
				strcat( temp_fname, EXTENSION_TEXT );
				free( *fname );
				*fname = temp_fname;
				return 1;
			} else {
				// Convert any extension to EXTENSION_NAME
				(*fname)[i] = 0;
				temp_fname = ( char * ) calloc ( strlen( *fname ) + strlen( EXTENSION_NAME ) + 1, sizeof( char ) );
				if( temp_fname == NULL ){
					printerr( "[ ChangeExtension ] out of memory" );
					return 0;
				}
				strcpy( temp_fname, *fname );
				strcat( temp_fname, EXTENSION_NAME );
				free( *fname );
				*fname = temp_fname;
				return 2;
			}
		}
	}
}

// Is the number a power of 2
// $ Return 1, if number is a power of 2
// $ Return 0, if not
inline int isPowOf2( int value ){
	return ( value > 0 && ( value & (value - 1) ) == 0 );
}

// Write global buffer to file
// $ dest - destination file
void Write( FILE *dest ){
	if( dest == NULL ){
		printerr( "[ Write ] unexpected null pointer\n" );
		return;
	}
	register char i;
	// Writing
	for( i = 0; i < push_pos / CHAR_BIT; i++ ){
		fputc( push_buffer[ i ], dest );
	}
	memset( push_buffer, 0, BUFFER_SIZE );
	push_pos = 0;
}

// Pushes hamming code to file [dest]
// $ [dest] - output stream
// $ [bit] - bit to push to output file
// $ [write] - write down existing bits? ( 0 - false, !0 - true )
void Push( FILE *dest, unsigned char bit, char write ){
	if( dest == NULL ){
		printerr( "[ Push ] unexpected null pointer" );
		return;
	}

	// Adding new bit to push buffer
	// $ Bits writes from right to left
	// $ Bytes writes from left to right
	push_pos += 1;
	push_buffer[ (push_pos - 1) / CHAR_BIT ] ^= bit << ( (CHAR_BIT - 1) - (push_pos - 1) % CHAR_BIT );
	
	// Writing
	if( write ){
		Write( dest );
	}
}

// Returns next bit from <source> file
// $ Returns -1, if end of file was found
// $ Returns -2, if error occurred
char Pop( FILE *source ){
	if( source == NULL ){
		printerr( "[ Pop ] unexpected null pointer" );
		return -2;
	}
	if( pop_pos == 0 ){
		pop_buffer = fgetc( source );
		if( feof( source ) ){
			return -1;
		}
		pop_pos = CHAR_BIT;
	}
	pop_pos -= 1;
	return ( ( pop_buffer & ( 1 << pop_pos ) ) && 1 );
}

// Encodes file <fin> and writes to <fout> with block size <block> ( in bits )
void Encode( FILE *fin, FILE *fout, short block ){
	if( fin == NULL || fout == NULL ){
		printerr( "[ Encode ] unexpected null pointer" );
		return;
	}

	// Iterators
	short i;
	short v;
	short ib = 1; // Block iterator

	// Buffer value	
	char buffer[ BUFFER_SIZE ];
	char bit_buf;

	while( 1 ){
		memset( buffer, 0, BUFFER_SIZE );
	
		// Info bits reading
		for( ib = 3; ib <= block; ib++ ){
			if( !isPowOf2( ib ) ){
				// Numberating starts from left to right
				bit_buf = Pop( fin );

				// End of the file or error
				if( feof( fin ) || bit_buf == -1 || bit_buf == -2 ){
					if( ib == 3 ){
						// Write remain of the buffer
						if( push_pos != 0 ){
							for( i = 1; i < block; i++ ){
								if( push_pos == 7 ){
									Push(
										fout, // $ [push_pos / CHAR_BIT + (push_pos % CHAR_BIT != 0)] - the last writed buffer's item
										( buffer[ (i - 1) / CHAR_BIT ] & ( 1 << ( (CHAR_BIT - 1) - ((i - 1) % CHAR_BIT) ) ) ) && 1,
										1 // Write a byte
									);
								} else {
									Push(
										fout, // $ [push_pos / CHAR_BIT + (push_pos % CHAR_BIT != 0)] - the last writed buffer's item
										( buffer[ (i - 1) / CHAR_BIT ] & ( 1 << ( (CHAR_BIT - 1) - ((i - 1) % CHAR_BIT) ) ) ) && 1,
										0 // Don't write a byte
									);
								}
							}
						}
						return;
					}
					break;
				}

				buffer[ ( ib - 1 ) / CHAR_BIT ] ^= bit_buf << ( (CHAR_BIT - 1) - (( ib - 1 ) % ( CHAR_BIT )) );
			}
		}

		// Control bits setting
		for( ib = 1; ib < block; ib *= 2 ){
			for( i = ib; i <= block; i += ib ){
				for( v = i; v < i + ib && v <= block; v++ ){
					// buffer[ <here> ] computes byte, using bit position
					if( GET_BIT( buffer[ (v - 1) / CHAR_BIT ], (CHAR_BIT - 1) - (( v - 1 ) % ( CHAR_BIT )) ) ){
						buffer[ (ib - 1) / CHAR_BIT ] ^= 1 << ( (CHAR_BIT - 1) - (( ib - 1 ) % ( CHAR_BIT )) );
					}
				}
				i = v;
			}
		}

		// Writing
		for( i = 1; i <= block; i++ ){
			if( push_pos == 7 ){
				Push(
					fout, // $ [push_pos / CHAR_BIT + (push_pos % CHAR_BIT != 0)] - the last writed buffer's item
					( buffer[ (i - 1) / CHAR_BIT ] & ( 1 << ( (CHAR_BIT - 1) - ((i - 1) % CHAR_BIT) ) ) ) && 1,
					1 // Write a byte
				);
			} else {
				Push(
					fout, // $ [push_pos / CHAR_BIT + (push_pos % CHAR_BIT != 0)] - the last writed buffer's item
					( buffer[ (i - 1) / CHAR_BIT ] & ( 1 << ( (CHAR_BIT - 1) - ((i - 1) % CHAR_BIT) ) ) ) && 1,
					0 // Don't write a byte
				);
			}
		}
	}
}

// Decodes file [fin] and writes to [fout] with block size [block] ( in bits )
void Decode( FILE *fin, FILE *fout, short block ){
	if( fin == NULL || fout == NULL ){
		printerr( "[ Encode ] unexpected null pointer" );
		return;
	}

	// Iterators
	short i = 0;
	short v = 0;
	short ib = 1; // Block iterator

	// Buffer value	
	char buffer[ BUFFER_SIZE ];
	char bit_buf;
	char error[ CTRL_BIT ];
	unsigned char error_count = 0;
	char ctrl_bit[ CTRL_BIT ];
	unsigned char bad_bit;
	char push_byte = 0; // What we write to output file
	push_pos = CHAR_BIT;

	while( 1 ){
		memset( buffer, 0, BUFFER_SIZE );
		memset( ctrl_bit, 0, CTRL_BIT );
		memset( error, 0, CTRL_BIT );
		bad_bit = 0;
		
		// Reading
		i = 0;
		for( ib = 1; ib <= block; ib++ ){
			bit_buf = Pop( fin );

			// End of the file or error
			if( feof( fin ) || bit_buf == -1 || bit_buf == -2 ){
				// Writing remain of the buffer
				for( i = 3; buffer[ (i - 1) / CHAR_BIT ]; i++ ){
					if( !isPowOf2( i ) ){
						push_pos -= 1;
						push_byte ^= ( ( buffer[ (i - 1) / CHAR_BIT ] & ( 1 << ( (CHAR_BIT - 1) - (( i - 1 ) % ( CHAR_BIT )) ) ) ) && 1 ) << push_pos;
						if( push_pos == 0 ){
							fputc( push_byte, fout );
							push_pos = CHAR_BIT;
							push_byte = 0;
						}
					}
				}
				return;
			}

			if( isPowOf2( ib ) && ib != block ){
				// Ctrl bits taking
				buffer[ ( ib - 1 ) / CHAR_BIT ] ^= 0 << ( (CHAR_BIT - 1) - (( ib - 1 ) % ( CHAR_BIT )) );
				ctrl_bit[ i ] = bit_buf;
				i++;
			} else {
				// Filling the buffer with info bits [ (block - ib) / CHAR_BIT ]
				buffer[ ( ib - 1 ) / CHAR_BIT ] ^= bit_buf << ( (CHAR_BIT - 1) - (( ib - 1 ) % ( CHAR_BIT )) );
			}
		}

		// Control bits setting
		for( ib = 1; ib < block; ib *= 2 ){
			for( i = ib; i <= block; i += ib ){
				for( v = i; v < i + ib && v <= block; v++ ){
					// buffer[ <here> ] computes byte, using bit position
					if( GET_BIT( buffer[ (v - 1) / CHAR_BIT ], (CHAR_BIT - 1) - (( v - 1 ) % ( CHAR_BIT )) ) ){
						buffer[ (ib - 1) / CHAR_BIT ] ^= 1 << ( (CHAR_BIT - 1) - (( ib - 1 ) % ( CHAR_BIT )) );
					}
				}
				i = v;
			}
		}

		// Ctrl bits comparison
		i = 0;
		for( ib = 1; ib < block; ib *= 2 ){
			if( ctrl_bit[ i ] != ( ( buffer[ (ib - 1) / CHAR_BIT ] & (1 << ( (CHAR_BIT - 1) - (( ib - 1 ) % ( CHAR_BIT )) )) ) && 1 ) ){
				error[ error_count ] = ib;
				error_count += 1;
			}
			i++;
		}

		// Error correction
		if( error_count ){
			// Invert bad bit
			for( i = 0; i < error_count; i++ ){
				// Computing position of bad bit
				bad_bit += error[ i ];
			}
			buffer[ (bad_bit - 1) / CHAR_BIT ] ^= 1 << ( (CHAR_BIT - 1) - (( bad_bit - 1 ) % ( CHAR_BIT )) );
			error_count = 0;
		}

		// Writing changes
		for( i = 3; i <= block; i++ ){
			if( !isPowOf2( i ) ){
				push_pos -= 1;
				push_byte ^= ( ( buffer[ (i - 1) / CHAR_BIT ] & ( 1 << ( (CHAR_BIT - 1) - (( i - 1 ) % ( CHAR_BIT )) ) ) ) && 1 ) << push_pos;
				if( push_pos == 0 ){
					fputc( push_byte, fout );
					push_pos = CHAR_BIT;
					push_byte = 0;
				}
			}
		}
		
		if( feof( fin ) || bit_buf == -1 || bit_buf == -2 ){
			return;
		}
	}
}

// #endregion