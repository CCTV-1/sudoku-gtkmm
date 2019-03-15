#pragma once
#ifndef SUDOKU_H
#define SUDOKU_H

#include <cstdint>

#include <map>
#include <string>
#include <vector>

typedef std::uint8_t cell_t;
static constexpr cell_t SUDOKU_SIZE = 9;
static constexpr cell_t SUDOKU_BOX_SIZE = 3;
typedef std::pair<cell_t , cell_t> postion_t;
typedef std::array< std::array< int8_t , SUDOKU_SIZE > , SUDOKU_SIZE > puzzle_t;
typedef std::vector<postion_t> answer_t;
typedef std::array< std::array< std::vector< cell_t > , SUDOKU_SIZE > , SUDOKU_SIZE > candidate_t;

enum class SUDOKU_LEVEL:std::uint8_t
{
    EASY = 0,
    MEDIUM,
    HARD,
    EXPERT,
    _LEVEL_COUNT,
};

class Sudoku
{
    public:
        Sudoku( puzzle_t puzzle , SUDOKU_LEVEL level = SUDOKU_LEVEL::EASY ) noexcept( false );
        Sudoku( SUDOKU_LEVEL level ) noexcept( false );

        Sudoku( const Sudoku& sudoku );
        Sudoku( Sudoku&& sudoku );
        Sudoku& operator=( const Sudoku& sudoku );
        Sudoku& operator=( Sudoku&& sudoku );
        ~Sudoku() = default;

        void fill_answer( std::size_t x , std::size_t y , std::size_t value ) noexcept( false );

        void erase_answer( std::size_t x , std::size_t y ) noexcept( false );

        void fill_candidates( std::size_t x , std::size_t y , std::vector<cell_t> candidates ) noexcept( false );

        void erase_candidates( std::size_t x , std::size_t y , std::vector<cell_t> candidates ) noexcept( false );

        bool is_answer( std::size_t x , std::size_t y ) noexcept( false );

        SUDOKU_LEVEL get_puzzle_level( void ) const noexcept( true );

        const candidate_t& get_candidates( void ) const noexcept( true );

        std::vector<puzzle_t> get_solution( bool need_all = false ) noexcept( false );

        const puzzle_t& get_puzzle( void ) const noexcept( true );

    private:
        SUDOKU_LEVEL level;
        puzzle_t puzzle;
        answer_t answer;
        candidate_t candidates;
        //std::vector<puzzle_t> solution;
};

std::string dump_level( SUDOKU_LEVEL level ) noexcept( false );

bool fill_check( const puzzle_t& puzzle , std::size_t x , std::size_t y ) noexcept( false );

//not usage solution check
bool check_puzzle( const puzzle_t& puzzle ) noexcept( false );

std::string dump_puzzle( const puzzle_t& puzzle ) noexcept( false );

//not thread safe
puzzle_t get_network_puzzle( enum SUDOKU_LEVEL level ) noexcept( false );

std::string dump_candidates( const candidate_t& candidates ) noexcept( true );

//modify puzzle (x,y) to value update candidate map
void update_candidates( candidate_t& candidates , const puzzle_t& puzzle , std::size_t x , std::size_t y ) noexcept( false );

candidate_t generate_candidates( const puzzle_t& puzzle ) noexcept( false );

bool operator==( const Sudoku& lhs , const Sudoku& rhs ) noexcept( true );

#endif