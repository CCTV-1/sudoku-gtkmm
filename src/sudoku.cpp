#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <jansson.h>

#include "dancinglinks.h"
#include "sudoku.h"

/* static std::map< SUDOKU_LEVEL , std::vector<puzzle_t> > builtin_sudoku_array =
{
    {
        SUDOKU_LEVEL::HARD,
        {
            {
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 2 , 7 , 1 , 0 , 0 , 0 , 0 , 8 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 0 , 0 , 7 , 0 , 0 , 4 , 2 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 8 , 0 , 0 , 0 , 0 , 0 , 9 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 1 , 0 , 0 , 0 , 0 , 0 , 6 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 5 , 0 , 0 , 0 , 2 , 0 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 0 , 0 , 8 , 5 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 7 , 1 , 4 , 0 , 0 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 4 , 0 , 2 , 6 , 0 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 3 }
            },
            {
                std::array< int8_t , SUDOKU_SIZE >{ 5 , 0 , 8 , 0 , 0 , 7 , 9 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 4 , 0 , 0 , 0 , 0 , 6 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 0 , 8 , 0 , 3 , 2 , 4 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 6 , 0 , 0 , 0 , 1 , 0 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 9 , 0 , 7 , 0 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 2 , 0 , 0 , 0 , 0 , 0 , 5 , 4 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 5 , 0 , 4 , 0 , 9 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 4 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 0 }
            },
            {
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 0 , 0 , 4 , 0 , 0 , 2 , 6 },
                std::array< int8_t , SUDOKU_SIZE >{ 3 , 0 , 9 , 7 , 2 , 0 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 5 , 0 , 0 , 0 , 0 , 4 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 7 , 9 , 1 , 3 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 6 , 0 , 3 , 0 , 0 , 0 , 0 , 0 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2 },
                std::array< int8_t , SUDOKU_SIZE >{ 1 , 0 , 0 , 0 , 0 , 0 , 0 , 8 , 0 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 0 , 0 , 0 , 0 , 8 , 0 , 0 , 9 },
                std::array< int8_t , SUDOKU_SIZE >{ 0 , 9 , 0 , 0 , 0 , 0 , 7 , 0 , 4 }
            }
        }
    }
}; */

static class LibCurlInit
{
    public:
        LibCurlInit()
        {
            if ( curl_global_init( CURL_GLOBAL_ALL ) )
            {
                throw std::runtime_error( "libcurl global initial failure" );
            };
        }
        ~LibCurlInit()
        {
            curl_global_cleanup();
        }
}_init_libcurl;

static std::size_t get_json_callback( char * content , std::size_t size , std::size_t element_number , void * save_ptr )
{
    std::size_t realsize = size*element_number;
    char ** ptr = static_cast< char** >( save_ptr );
    *ptr = static_cast<char *>( malloc( sizeof( char )*realsize + 1 ) );
    memccpy( *ptr , content , 1 , realsize );
    ( *ptr )[realsize] = '\0';
    return realsize;
}

Sudoku::Sudoku( puzzle_t puzzle , SUDOKU_LEVEL level ) noexcept( false )
{
    std::string except_message( __func__ );

    if ( level >= SUDOKU_LEVEL::_LEVEL_COUNT )
    {
        except_message += ":unknown puzzle level";
        throw std::out_of_range( except_message );
    }
    if ( check_puzzle( puzzle ) == false )
    {
        except_message += ":puzzle illegal";
        throw std::invalid_argument( except_message );
    }
    this->level = level;
    this->puzzle = puzzle;
    this->answer = {};
    this->candidates = generate_candidates( this->puzzle );
}

Sudoku::Sudoku( SUDOKU_LEVEL level ) noexcept( false ) :
    Sudoku::Sudoku( get_network_puzzle( level ) , level ) 
{
    ;
}

Sudoku::Sudoku( const Sudoku& sudoku ):
    level( sudoku.level ),
    puzzle( sudoku.puzzle ),
    answer( sudoku.answer ),
    candidates( sudoku.candidates )
{
    ;
}

Sudoku::Sudoku( Sudoku&& sudoku )
{
    this->level = std::move( sudoku.level );
    this->puzzle = std::move( sudoku.puzzle );
    this->answer = std::move( sudoku.answer );
    this->candidates = std::move( sudoku.candidates );
}

Sudoku& Sudoku::operator=( const Sudoku& sudoku )
{
    this->level = sudoku.level;
    this->puzzle = sudoku.puzzle;
    this->answer = sudoku.answer;
    this->candidates = sudoku.candidates;
    return *this;
}

Sudoku& Sudoku::operator=( Sudoku&& sudoku )
{
    if ( this != &sudoku )
    {
        this->level = std::move( sudoku.level );
        this->puzzle = std::move( sudoku.puzzle );
        this->answer = std::move( sudoku.answer );
        this->candidates = std::move( sudoku.candidates );
    }
    return *this;
}

void Sudoku::fill_answer( std::size_t x , std::size_t y , std::size_t value ) noexcept( false )
{
    std::string except_message( __func__ );

    //std::size_t is unsigned interger the value >= 0;
    if ( x > SUDOKU_SIZE )
    {
        except_message += ":argument x value:" + std::to_string( x );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    //std::size_t is unsigned interger the value >= 0;
    if ( y > SUDOKU_SIZE )
    {
        except_message += ":argument y value:" + std::to_string( y );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }

    postion_t pos = { x , y };
    auto answer_iter = std::find( this->answer.begin() , this->answer.end() , pos );
    if ( ( this->puzzle[x][y] != 0 ) && ( answer_iter == this->answer.end() ) )
    {
        except_message += ":cell:( " + std::to_string( x );
        except_message += " , " + std::to_string( y ) + " ) is puzzle content,can't be modify";
        throw std::out_of_range( except_message );
    }
    if ( value > SUDOKU_SIZE )
    {
        except_message += ":argument value value:( " + std::to_string( value );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }

    auto temp = this->puzzle[x][y];
    this->puzzle[x][y] = value;
    if ( fill_check( this->puzzle , x , y ) == false )
    {
        //backup puzzle
        this->puzzle[x][y] = temp;
        except_message += ":puzzle illegal";
        throw std::invalid_argument( except_message );
    }

    if ( value == 0 )
    {
        if ( answer_iter != this->answer.end() )
            this->answer.erase( answer_iter );
    }
    else
    {
        if ( answer_iter == this->answer.end() )
            this->answer.push_back( pos );
    }
    //update_candidates( this->candidates , this->puzzle , x , y );
}

void Sudoku::erase_answer( std::size_t x , std::size_t y ) noexcept( false )
{
    this->fill_answer( x , y  , 0 );
}

bool Sudoku::is_answer( std::size_t x , std::size_t y ) noexcept( false )
{
    std::string except_message( __func__ );

    //std::size_t is unsigned interger the value >= 0;
    if ( x > SUDOKU_SIZE )
    {
        except_message += ":argument x value:" + std::to_string( x );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    //std::size_t is unsigned interger the value >= 0;
    if ( y > SUDOKU_SIZE )
    {
        except_message += ":argument y value:" + std::to_string( y );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    postion_t pos = { x , y };

    return ( std::find( this->answer.begin() , this->answer.end() , pos ) != this->answer.end() );
}

void Sudoku::fill_candidates( std::size_t x , std::size_t y , std::vector<cell_t> candidates ) noexcept( false )
{
    std::string except_message( __func__ );

    //std::size_t is unsigned interger the value >= 0;
    if ( x > SUDOKU_SIZE )
    {
        except_message += ":argument x value:" + std::to_string( x );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    //std::size_t is unsigned interger the value >= 0;
    if ( y > SUDOKU_SIZE )
    {
        except_message += ":argument y value:" + std::to_string( y );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    if ( candidates.empty() )
        return ;

    for ( auto value : candidates )
    {
        if ( value >SUDOKU_SIZE)
        {
            continue;
        }
        auto value_iter = std::find( this->candidates[x][y].begin() , this->candidates[x][y].end() , value );
        if ( value_iter == this->candidates[x][y].end() )
            this->candidates[x][y].push_back( value );
    }

}

void Sudoku::erase_candidates( std::size_t x , std::size_t y , std::vector<cell_t> candidates ) noexcept( false )
{
    std::string except_message( __func__ );

    //std::size_t is unsigned interger the value >= 0;
    if ( x > SUDOKU_SIZE )
    {
        except_message += ":argument x value:" + std::to_string( x );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    //std::size_t is unsigned interger the value >= 0;
    if ( y > SUDOKU_SIZE )
    {
        except_message += ":argument y value:" + std::to_string( y );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    if ( candidates.empty() )
        return ;
    
    for ( auto value : candidates )
    {
        if ( value >SUDOKU_SIZE)
        {
            continue;
        }
        auto value_iter = std::find( this->candidates[x][y].begin() , this->candidates[x][y].end() , value );

        if ( value_iter != this->candidates[x][y].end() )
        {
            this->candidates[x][y].erase( value_iter );
        }
    }
}

SUDOKU_LEVEL Sudoku::get_puzzle_level( void ) const noexcept( true )
{
    return this->level;
}

const candidate_t& Sudoku::get_candidates( void ) const noexcept( true )
{
    return this->candidates;
}

std::vector<puzzle_t> Sudoku::get_solution( bool need_all ) noexcept( false )
{
    //every placement of a number in a position is a subset --> 9*9*9
    constexpr std::size_t rows = SUDOKU_SIZE*SUDOKU_SIZE*SUDOKU_SIZE;
    //a cell can have only 1 number : 9*9 constraints
    //a row can have a number only once : 9*9 constraints
    //a column can have a number only once : 9*9 constraints
    //a box can have a number only once : 9*9 constraints
    constexpr std::size_t columns = SUDOKU_SIZE*SUDOKU_SIZE*4;
    auto get_box_index = []( size_t x , size_t y ) -> std::size_t { return ( x/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + y/SUDOKU_BOX_SIZE; };
    std::array<int32_t,rows> disallow_row;
    disallow_row.fill( 1 );
    std::array<int32_t,rows> disallow_column;
    disallow_column.fill( 1 );
    
    for ( std::size_t i = 0 ; i < SUDOKU_SIZE ; i ++ )
    {
        for ( std::size_t j = 0 ; j < SUDOKU_SIZE ; j++ )
        {
            //allowed cell
            if ( this->puzzle[i][j] == 0 )
                continue;
            for ( std::size_t k = 0 ; k < SUDOKU_SIZE ; k++ )
            {
                //other numbers can't in the same cell
                disallow_row[ i*SUDOKU_SIZE*SUDOKU_SIZE + j*SUDOKU_SIZE + k ] = 0;
                //this->puzzle[i][j] can't appear in another column in the same row
                disallow_row[ i*SUDOKU_SIZE*SUDOKU_SIZE + k*SUDOKU_SIZE + this->puzzle[i][j] - 1 ] = 0;
                //this->puzzle[i][j] can't appear in another row in the same column
                disallow_row[ k*SUDOKU_SIZE*SUDOKU_SIZE + j*SUDOKU_SIZE + this->puzzle[i][j] - 1 ] = 0;
                //this->puzzle[i][j] can't appear in another cell in the same box
                disallow_row[ get_box_index( i , k )*SUDOKU_SIZE*SUDOKU_SIZE + get_box_index( j , k )*SUDOKU_SIZE + this->puzzle[i][j] - 1 ] = 0;
            }
            //cell constraint satisfied
            disallow_column[ i*SUDOKU_SIZE + j ] = 0;
            //row constraint satisfied
            disallow_column[ 1*SUDOKU_SIZE*SUDOKU_SIZE + i*SUDOKU_SIZE + this->puzzle[i][j] - 1 ] = 0;
            //colum constraint satisfied
            disallow_column[ 2*SUDOKU_SIZE*SUDOKU_SIZE + j*SUDOKU_SIZE + this->puzzle[i][j] - 1 ] = 0;
            //box constraint satisfied
            disallow_column[ 3*SUDOKU_SIZE*SUDOKU_SIZE + get_box_index( i , j )*SUDOKU_SIZE + this->puzzle[i][j] - 1 ] = 0;;
        }
    }

    std::array<int32_t,rows> inverse_disallow_column;
    //DLX matrix R
    std::int32_t R = 0;
    //DLX matrix C
    std::int32_t C = 0;
    for ( std::size_t i = 0 ; i < rows ; i++ )
    {
        if ( disallow_row[i] != 0 )
        {
            disallow_row[i] = R;
            inverse_disallow_column[R] = i;
            R++;
        }
        else
            disallow_row[i] = -1;
    }
    for ( std::size_t i = 0 ; i < columns ; i++ )
    {
        if ( disallow_column[i] )
            disallow_column[i] = C++;
        else
            disallow_column[i] = -1;
    }

    //create store the constraint adjacency matrix
    std::shared_ptr<bool[]> matrix( new bool[R*C]() );
    for ( std::size_t i = 0; i < SUDOKU_SIZE; i++)
    {
        for ( std::size_t j = 0; j < SUDOKU_SIZE; j++)
        {
            for ( std::size_t k = 0; k < SUDOKU_SIZE; k++)
            {
                std::size_t r = i*SUDOKU_SIZE*SUDOKU_SIZE + j*SUDOKU_SIZE + k;
                if ( disallow_row[r] == -1 )
                    continue;
                std::size_t index1 = i*SUDOKU_SIZE + j;
                std::size_t index2 = 1*SUDOKU_SIZE*SUDOKU_SIZE + i*SUDOKU_SIZE + k;
                std::size_t index3 = 2*SUDOKU_SIZE*SUDOKU_SIZE + j*SUDOKU_SIZE + k;
                std::size_t index4 = 3*SUDOKU_SIZE*SUDOKU_SIZE + get_box_index( i , j )*SUDOKU_SIZE + k;
                if ( disallow_column[index1] != -1 )
                    matrix.get()[ disallow_row[r]*C + disallow_column[index1] ] = 1;
                if ( disallow_column[index2] != -1 )
                    matrix.get()[ disallow_row[r]*C + disallow_column[index2] ] = 1;
                if ( disallow_column[index3] != -1 )
                    matrix.get()[ disallow_row[r]*C + disallow_column[index3] ] = 1;
                if ( disallow_column[index4] != -1 )
                    matrix.get()[ disallow_row[r]*C + disallow_column[index4] ] = 1;
            }
        }
    }

    DancingLinks N;
    N.create( R , C , matrix.get() );
    std::vector< std::vector<std::int32_t> > all_solution;
    std::vector<std::int32_t> current_solution;
    N.solve( all_solution , current_solution , need_all );
    std::vector< puzzle_t > results = {};
    for ( auto solution : all_solution )
    {
        puzzle_t result = this->puzzle;
        for ( std::size_t i = 0; i < solution.size() ; i++ )
        {
            std::size_t x = inverse_disallow_column[ solution[i] ];
            result[ x/( SUDOKU_SIZE*SUDOKU_SIZE ) ][ (x/SUDOKU_SIZE)%SUDOKU_SIZE ] = x%9 + 1;
        }
        results.push_back( result );
    }    
    return results;
}

const puzzle_t& Sudoku::get_puzzle( void ) const noexcept( true )
{
    return this->puzzle;
}

std::string dump_level( SUDOKU_LEVEL level ) noexcept( false )
{
    std::string except_message( __func__ );

    if ( level >= SUDOKU_LEVEL::_LEVEL_COUNT )
    {
        except_message += ":unknown puzzle level";
        throw std::out_of_range( except_message );
    }

    std::string result;
    switch ( level )
    {
        case SUDOKU_LEVEL::EASY:
            result += "easy";
            break;
        case SUDOKU_LEVEL::MEDIUM:
            result += "medium";
            break;
        case SUDOKU_LEVEL::HARD:
            result += "hard";
            break;
        case SUDOKU_LEVEL::EXPERT:
            result += "expert";
            break;
        default:
            break;
    }

    return result;
}

bool fill_check( const puzzle_t& puzzle , std::size_t x , std::size_t y ) noexcept( false )
{
    std::string except_message( __func__ );

    if ( puzzle.empty() )
    {
        except_message += ":puzzle is empty";
        throw std::invalid_argument( except_message );
    }
    //std::size_t is unsigned interger the value >= 0;
    if ( x > SUDOKU_SIZE )
    {
        except_message += ":argument x value:" + std::to_string( x );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    //std::size_t is unsigned interger the value >= 0;
    if ( y > SUDOKU_SIZE )
    {
        except_message += ":argument y value:" + std::to_string( y );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    if ( puzzle[x][y] < 0 || puzzle[x][y] > SUDOKU_SIZE )
    {
        except_message += ":cell:( " + std::to_string( x );
        except_message += " , " + std::to_string( y ) + " ) value:" + std::to_string( puzzle[x][y] );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }

    std::map<cell_t , cell_t> columns_map;
    std::map<cell_t , cell_t> row_map;
    std::map<cell_t , cell_t> box_map;
    for ( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        cell_t row_number = puzzle[x][i];
        cell_t column_number = puzzle[i][y];
        cell_t box_number = puzzle[( x/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + i/SUDOKU_BOX_SIZE ][ ( y/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + i%SUDOKU_BOX_SIZE ];
        columns_map[row_number]++;
        row_map[column_number]++;
        box_map[box_number]++;
        if ( ( columns_map[row_number] > 1 ) && ( row_number != 0 ) )
        {
            return false;
        }
        if ( ( row_map[column_number] > 1 ) && ( column_number != 0 ) )
        {
            return false;
        }
        if ( ( box_map[box_number] > 1 ) && ( box_number != 0 ) )
        {
            return false;
        }
    }
    return true;
}

bool check_puzzle( const puzzle_t& puzzle ) noexcept( false )
{
    std::string except_message( __func__ );

    if ( puzzle.empty() )
    {
        except_message += ":puzzle is empty";
        throw std::invalid_argument( except_message );
    }

    std::array< std::map< cell_t , cell_t > , SUDOKU_SIZE > columns_map;
    std::array< std::map< cell_t , cell_t > , SUDOKU_SIZE > row_map;
    std::array< std::map< cell_t , cell_t > , SUDOKU_SIZE > box_map;
    for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
        {
            int8_t number = puzzle[i][j];
            if ( number < 0 || number > SUDOKU_SIZE )
            {
                except_message += ":cell:( " + std::to_string( i );
                except_message += " , " + std::to_string( j ) + " ) value:" + std::to_string( number );
                except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
                throw std::out_of_range( except_message );
            }
            cell_t box_index = ( i/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + j/SUDOKU_BOX_SIZE;
            if ( number == 0 )
                continue;
            columns_map[i][number]++;
            row_map[j][number]++;
            box_map[box_index][number]++;
            if ( columns_map[i][number] > 1 || row_map[j][number] > 1 || box_map[box_index][number] > 1 )
            {
                return false;
            }
        }
    }
    return true;
}

std::string dump_puzzle( const puzzle_t& puzzle ) noexcept( false )
{
    std::string result;
    std::string except_message( __func__ );
    
    if ( check_puzzle( puzzle ) == false )
    {
        except_message += "illegal puzzle,can't dump to string";
        throw std::invalid_argument( except_message );
    }
    for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
        {
            result += std::to_string( puzzle[i][j] );
            result += " ";
        }
        result += '\n';
    }
    return result;
}

puzzle_t get_network_puzzle( SUDOKU_LEVEL level ) noexcept( false )
{
    std::string except_message( __func__ );

    //API:https://sudoku.com/api/getLevel/$(Level)
    std::string api_url( "https://sudoku.com/api/getLevel/" + dump_level( level ) );

    std::int8_t re_try = 4;
    long default_timeout = 30L;
    char * json_buff = nullptr;
    char error_buff[1024];
    CURLcode res = CURLE_OK;
    std::shared_ptr<CURL> curl_handle( curl_easy_init() , curl_easy_cleanup );
    if ( curl_handle.get() == nullptr )
    {
        except_message += ":libcurl easy initial failure";
        throw std::runtime_error( except_message );
    }
    do
    {
        curl_easy_setopt( curl_handle.get() , CURLOPT_URL , api_url.c_str() );
        curl_easy_setopt( curl_handle.get() , CURLOPT_VERBOSE , 0L );
        curl_easy_setopt( curl_handle.get() , CURLOPT_FOLLOWLOCATION , 1L );
        curl_easy_setopt( curl_handle.get() , CURLOPT_USE_SSL , 1L );
        curl_easy_setopt( curl_handle.get() , CURLOPT_NOPROGRESS , 1L );
        curl_easy_setopt( curl_handle.get() , CURLOPT_TIMEOUT , default_timeout );
        curl_easy_setopt( curl_handle.get() , CURLOPT_WRITEFUNCTION , get_json_callback );
        curl_easy_setopt( curl_handle.get() , CURLOPT_WRITEDATA , &json_buff );
        curl_easy_setopt( curl_handle.get() , CURLOPT_ERRORBUFFER , error_buff );           
        res = curl_easy_perform( curl_handle.get() );
        if ( res != CURLE_OK )
        {
            if ( re_try == 0 )
            {
                except_message += ":get network puzzle failure,libcurl error message:";
                except_message += error_buff;
                throw std::runtime_error( except_message );
            }
            default_timeout += 10L;
            re_try--;
        }
    }
    while ( res != CURLE_OK );

    json_error_t error;
    std::shared_ptr<json_t> root( json_loads( json_buff , 0 , &error ) , json_decref );
    if ( root.get() == nullptr )
    {
        except_message += "newtwork json:'";
        except_message += json_buff;
        except_message += "' format does not meet expectations";
        throw std::invalid_argument( except_message );
    }
    //{
    //    "answer": "success",
    //    "message": "Level exist",
    //    "desc": [
    //        "008000402000320780702506000003050004009740200006200000000000500900005600620000190",   //puzzle
    //        "368179452591324786742586319213658974859743261476291835187962543934815627625437198",   //solution
    //        310,
    //        7,
    //        false
    //    ]
    //}
    json_t * desc_array = json_object_get( root.get() , "desc" );
    if ( json_is_array( desc_array ) == false )
    {
        except_message += "newtwork json:'";
        std::shared_ptr<char> jsons( json_dumps( desc_array , JSON_INDENT( 4 ) ) , free );
        except_message += jsons.get();
        except_message += "' don't exist puzzle";
        throw std::invalid_argument( except_message );
    }
    json_t * puzzle_node = json_array_get( desc_array , 0 );
    if ( json_is_string( puzzle_node )  == false )
    {
        except_message += "newtwork json:'";
        std::shared_ptr<char> jsons( json_dumps( puzzle_node , JSON_INDENT( 4 ) ) , free );
        except_message += jsons.get();
        except_message += "' don't exist puzzle";
        throw std::invalid_argument( except_message );
    }
    const char * puzzle_str = json_string_value( puzzle_node );
    if ( strnlen( puzzle_str , SUDOKU_SIZE*SUDOKU_SIZE*2 ) != SUDOKU_SIZE*SUDOKU_SIZE )
    {
        except_message += "puzzle:'";
        except_message += puzzle_str;
        except_message += "'  size failure";
        throw std::length_error( except_message );
    }
    
    puzzle_t result;
    for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
        {
            result[i][j] = puzzle_str[ i*SUDOKU_SIZE + j ] - '0';
        }
    }

    free( json_buff );
    return result;
}

std::string dump_candidates( const candidate_t& candidates ) noexcept( true )
{
    std::string result;
    for ( cell_t i = 0 ; i < SUDOKU_SIZE * ( SUDOKU_BOX_SIZE*2 + 1 ) ; i++ )
    {
        result += '-';
    }
    result += '\n';
    for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        std::array< std::string , SUDOKU_BOX_SIZE > lines;
        for ( cell_t k = 0 ; k < SUDOKU_BOX_SIZE ; k++ )
        {
            lines[k] += "|";
        }
        for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
        {
            for ( cell_t k = 0 ; k < SUDOKU_SIZE ; k++ )
            {
                if ( std::find( candidates[i][j].begin() , candidates[i][j].end() , k + 1 ) != candidates[i][j].end() )
                {
                    lines[ k/SUDOKU_BOX_SIZE ] += std::to_string( k + 1 );
                }
                else
                {
                    lines[ k/SUDOKU_BOX_SIZE ] += " ";
                }
                lines[ k/SUDOKU_BOX_SIZE ] += " ";
            }
            for ( cell_t k = 0 ; k < SUDOKU_BOX_SIZE ; k++ )
            {
                lines[k] += "|";
            }
        }
        for ( cell_t k = 0 ; k < SUDOKU_BOX_SIZE ; k++ )
        {
            result += lines[ k ];
            result += '\n';
        }
        for ( cell_t k = 0 ; k < SUDOKU_SIZE * ( SUDOKU_BOX_SIZE*2 + 1 ) ; k++ )
        {
            result += '-';
        }
        result += '\n';
    }
    return result;
}

//modify puzzle (x,y) to value update candidate map 
void update_candidates( candidate_t& candidates , const puzzle_t& puzzle , std::size_t x , std::size_t y ) noexcept( false )
{
    std::string except_message( __func__ );

    if ( check_puzzle( puzzle ) == false )
    {
        except_message += "puzzle illegal";
        throw std::invalid_argument( except_message );
    }
    //std::size_t is unsigned interger the value >= 0;
    if ( x > SUDOKU_SIZE )
    {
        except_message += ":argument x value:" + std::to_string( x );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }
    //std::size_t is unsigned interger the value >= 0;
    if ( y > SUDOKU_SIZE )
    {
        except_message += ":argument y value:" + std::to_string( y );
        except_message += ",out of range [ 0 , " + std::to_string( SUDOKU_SIZE ) + " ]";
        throw std::out_of_range( except_message );
    }


    cell_t number = puzzle[x][y];
    auto remove_condidate = [ &number ]( decltype( candidates[0][0] ) condidates ) -> void 
    {
        auto condidate_iterator = std::find( condidates.begin() , condidates.end() , number );
        if ( condidate_iterator != condidates.end() )
        {
            condidates.erase( condidate_iterator );
        } 
    };
    for ( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        remove_condidate( candidates[x][i] );
        remove_condidate( candidates[i][y] );
        remove_condidate( candidates[( x/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + i/SUDOKU_BOX_SIZE ][ ( y/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + i%SUDOKU_BOX_SIZE ] );
    }
}

candidate_t generate_candidates( const puzzle_t& puzzle ) noexcept( false )
{
    std::string except_message( __func__ );

    if ( check_puzzle( puzzle ) == false )
    {
        except_message += "puzzle illegal";
        throw std::invalid_argument( except_message );
    }
    //if SUDOKU_SIZE == 9 : 0 - SUDOKU_SIZE element , 0X(ignore bit)111111111 disallow [1-9]
    std::array< std::uint64_t , SUDOKU_SIZE > disallow_columns;
    std::array< std::uint64_t , SUDOKU_SIZE > disallow_row;
    std::array< std::uint64_t , SUDOKU_SIZE > disallow_box;
    disallow_columns.fill( 0 );
    disallow_row.fill( 0 );
    disallow_box.fill( 0 );

    for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
        {
            cell_t number = puzzle[i][j];
            if ( number == 0 )
                continue;
            cell_t box_index = ( i/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + j/SUDOKU_BOX_SIZE;
            disallow_columns[i] |= ( static_cast<std::uint64_t>(1) << ( number - 1 ) );
            disallow_row[j] |= ( static_cast<std::uint64_t>(1) << ( number - 1 ) );
            disallow_box[box_index] |= ( static_cast<std::uint64_t>(1) << ( number - 1 ) );
        }
    }
    candidate_t result;
    for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
        {
            cell_t number = puzzle[i][j];
            if ( number != 0 )
            {
                continue;
            }
            cell_t box_index = ( i/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + j/SUDOKU_BOX_SIZE;
            std::uint64_t allow_mark = ( ~( ( disallow_columns[i] | disallow_row[j] ) | disallow_box[box_index] ) );
            for ( cell_t number = 0 ; number < SUDOKU_SIZE ; number++ )
            {
                if ( ( allow_mark & ( static_cast<std::uint64_t>(1) << number ) ) != 0 )
                {
                    result[i][j].push_back( number + 1 );
                }
                
            }
        }
    }
    return result;
}

bool operator==( const Sudoku& lhs , const Sudoku& rhs ) noexcept( true )
{
    return ( lhs.get_puzzle() == rhs.get_puzzle() );
}

bool operator!=( const Sudoku& lhs , const Sudoku& rhs ) noexcept( true )
{
    return !( lhs == rhs );
}
