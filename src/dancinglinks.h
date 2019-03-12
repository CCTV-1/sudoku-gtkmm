#pragma once
#ifndef DANCINGLINKS_H
#define DANCINGLINKS_H

#include <cstdint>
#include <vector>

class DancingLinksCell;
class DancingLinksColumn;

// 1-elements in the adjacency list
class DancingLinksCell
{
    public:
        DancingLinksCell();
        ~DancingLinksCell() = default;

        DancingLinksCell * get_left_ptr( void )
        {
            return this->left_ptr;
        }
        DancingLinksCell * get_right_ptr( void )
        {
            return this->right_ptr;
        }
        DancingLinksCell * get_up_ptr( void )
        {
            return this->up_ptr;
        }
        DancingLinksCell * get_down_ptr( void )
        {
            return this->down_ptr;
        }

        DancingLinksColumn * get_column_ptr( void )
        {
            return this->column_ptr;
        }

        std::int32_t get_row_id( void )
        {
            return this->row_id;
        }

        void set_left_ptr( DancingLinksCell * ptr )
        {
            this->left_ptr = ptr;
        }
        void set_right_ptr( DancingLinksCell * ptr )
        {
            this->right_ptr = ptr;
        }
        void set_up_ptr( DancingLinksCell * ptr )
        {
            this->up_ptr = ptr;
        }
        void set_down_ptr( DancingLinksCell * ptr )
        {
            this->down_ptr = ptr;
        }

        void set_column_ptr( DancingLinksColumn * ptr )
        {
            this->column_ptr = ptr;
        }
        void set_row_id( int row_id )
        {
            this->row_id = row_id;
        }

        void remove_cell( void );
        void unremove_cell( void );
        void erase( void );
    protected:
        DancingLinksCell * left_ptr;
        DancingLinksCell * right_ptr;
        DancingLinksCell * up_ptr;
        DancingLinksCell * down_ptr;

        //pointer to the current column
        DancingLinksColumn * column_ptr;
        //ID of the current row
        std::int32_t row_id;
};

class DancingLinksColumn
{
    public:
        DancingLinksColumn();
        ~DancingLinksColumn();
        
        DancingLinksColumn * get_left_ptr( void )
        {
            return this->left_ptr;
        }
        DancingLinksColumn * get_right_ptr( void )
        {
            return this->right_ptr;
        }

        DancingLinksCell * get_dummy_cell()
        {
            return this->dummycell;
        }

        std::size_t get_size()
        {
            return this->size;
        }
        bool get_essential()
        {
            return this->essential;
        }

        void set_left_ptr( DancingLinksColumn * ptr )
        {
            this->left_ptr = ptr;
        }
        void set_right_ptr( DancingLinksColumn * ptr )
        {
            this->right_ptr = ptr;
        }

        void set_essential( bool flag )
        {
            this->essential = flag;
        }
        void set_size( std::int32_t size )
        {
            this->size = size;
        }
        void increment_size( void )
        {
            this->size++;
        }
        void decrement_size( void )
        {
            this->size--;
        }

        void remove_column();
        void unremove_column();
    protected:
        // Horizontal neighbours
        DancingLinksColumn * left_ptr;
        DancingLinksColumn * right_ptr;

        // Dummy cell to ease vertical movement
        DancingLinksCell * dummycell;

        // Number of non-zero elements in the column
        std::size_t size;

        // Whether the column is an essential constraint (Number of 1-elements in
        //  the column == 1) or a non-essential constraint (number of 1-elements in
        //  the column <= 1)
        bool essential;
};

class DancingLinks
{
    public:
        DancingLinks();
        ~DancingLinks();

        void create( std::int32_t R , std::int32_t C , bool * matrix );

        void destroy( void );

        bool solve( std::vector<std::vector<std::int32_t>> &allsolutions , std::vector<int32_t>& current_solution , bool need_all = false );
    private:
        DancingLinksColumn * dummy_column;
        DancingLinksColumn * columns;
        DancingLinksCell * cells;
};

#endif