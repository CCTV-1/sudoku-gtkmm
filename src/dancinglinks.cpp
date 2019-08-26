#include "dancinglinks.h"

#include <stdexcept>

DancingLinksCell::DancingLinksCell()
{
    this->left_ptr = this->right_ptr = this->up_ptr = this->down_ptr = nullptr;
    this->column_ptr = nullptr;
    this->row_id = -1;
}

void DancingLinksCell::remove_cell( void )
{
    this->up_ptr->set_down_ptr( this->down_ptr );
    this->down_ptr->set_up_ptr( this->up_ptr );
    this->column_ptr->decrement_size();
}

void DancingLinksCell::unremove_cell( void )
{
    this->up_ptr->set_down_ptr( this );
    this->down_ptr->set_up_ptr( this );
    this->column_ptr->increment_size();
}

void DancingLinksCell::erase( void )
{
    this->remove_cell();
    this->left_ptr->set_right_ptr( this->right_ptr );
    this->right_ptr->set_left_ptr( this->left_ptr );
}

DancingLinksColumn::DancingLinksColumn()
{
    this->left_ptr = this->right_ptr = nullptr;
    this->dummycell = new DancingLinksCell;
    this->size = 0;
    this->essential = true;
}

DancingLinksColumn::~DancingLinksColumn()
{
    delete this->dummycell;
}

void DancingLinksColumn::remove_column( void )
{
    // Remove the column from the column list
    this->left_ptr->set_right_ptr( this->right_ptr );
    this->right_ptr->set_left_ptr( this->left_ptr );

    // Find the 1-cells in the column and remove the other 1-cells in those rows
    //  from their respective columns
    for ( DancingLinksCell * ptr = this->dummycell->get_down_ptr() ; ptr != this->dummycell ; ptr = ptr->get_down_ptr() )
    {
        // Found a 1-cell in the column
        // Erase the row by finding its row neighbours and removing their
        //  vertical links
        for ( DancingLinksCell * ptr2 = ptr->get_right_ptr() ; ptr2 != ptr ; ptr2 = ptr2->get_right_ptr() )
        {
            // 1-cell in the row
            // Remove vertical links
            ptr2->remove_cell();
        }
    }
}

void DancingLinksColumn::unremove_column( void )
{
    // Put back the cells in the reverse order as they were removed
    for ( DancingLinksCell * ptr = this->dummycell->get_up_ptr() ; ptr != this->dummycell ; ptr = ptr->get_up_ptr() )
    {
        for ( DancingLinksCell * ptr2 = ptr->get_left_ptr() ; ptr2 != ptr ; ptr2 = ptr2->get_left_ptr() )
        {
            ptr2->unremove_cell();
        }
    }

    // Insert the column to the column list
    this->left_ptr->set_right_ptr( this );
    this->right_ptr->set_left_ptr( this );
}

DancingLinks::DancingLinks()
{
    this->dummy_column = new DancingLinksColumn;
    this->dummy_column->set_essential( false );
    this->columns = nullptr;
    this->cells = nullptr;
}

DancingLinks::~DancingLinks()
{
    this->destroy();
    delete this->dummy_column;
}

void DancingLinks::create( std::int32_t R , std::int32_t C , bool * matrix )
{
    if ( R <= 0 )
    {
        throw std::out_of_range( "R should be greater than zero" );
    }
    if ( C <= 0 )
    {
        throw std::out_of_range( "C should be greater than zero" );
    }

    //cleansing old struct if exist
    this->destroy();

    //overdose alloc,fast.
    this->columns = new DancingLinksColumn[C];

    this->dummy_column->set_right_ptr( &this->columns[0] );
    this->dummy_column->set_left_ptr( &this->columns[C - 1] );
    this->columns[0].set_left_ptr( this->dummy_column );
    this->columns[C - 1].set_right_ptr( this->dummy_column );

    for ( std::int32_t i = 0 ; i < C - 1 ; i++ )
    {
        this->columns[i].set_right_ptr( &this->columns[i + 1] );
        this->columns[i + 1].set_left_ptr( &this->columns[i] );
    }

    for ( std::int32_t i = 0 ; i < C ; i++ )
    {
        this->columns[i].set_size( R );
        this->columns[i].set_essential( i < C );
    }

    ////overdose alloc,fast.
    this->cells = new DancingLinksCell[R * C];

    for ( std::int32_t i = 0, k = 0 ; i < R ; i++ )
    {
        for ( std::int32_t j = 0 ; j < C ; j++, k++ )
        {
            this->cells[k].set_row_id( i );
            this->cells[k].set_column_ptr( &this->columns[j] );

            if ( k%C == 0 )
                this->cells[k].set_left_ptr( &this->cells[k + C-1] );
            else
                this->cells[k].set_left_ptr( &this->cells[k-1] );

            if ( k%C == C-1 )
                this->cells[k].set_right_ptr( &this->cells[k-C+1] );
            else
                this->cells[k].set_right_ptr( &this->cells[k+1] );

            if ( k < C )
            {
                this->cells[k].set_up_ptr( this->columns[j].get_dummy_cell() );
                this->columns[j].get_dummy_cell()->set_down_ptr( &this->cells[k] );
            }
            else
                this->cells[k].set_up_ptr( &this->cells[k - C] );

            if ( k >= ( (R-1)*C ) )
            {
                this->cells[k].set_down_ptr( this->columns[j].get_dummy_cell() );
                this->columns[j].get_dummy_cell()->set_up_ptr( &this->cells[k] );
            }
            else
                this->cells[k].set_down_ptr( &this->cells[k + C] );
        }
    }

    for ( std::int32_t i = 0 , k = 0 ; i < R ; i++ )
    {
        for ( std::int32_t j = 0 ; j < C ; j++ , k++ )
        {
            if ( matrix[k] == false )
                this->cells[k].erase();
        }
    }
}

void DancingLinks::destroy( void )
{
    if ( this->columns != nullptr )
    {
        delete[] this->columns;
        this->columns = nullptr;
    }
    if ( this->cells != nullptr )
    {
        delete[] this->cells;
        this->cells = nullptr;
    }
    this->dummy_column->set_left_ptr( nullptr );
    this->dummy_column->set_right_ptr( nullptr );
}

//https://en.wikipedia.org/wiki/Knuth%27s_Algorithm_X
bool DancingLinks::solve( std::vector<std::vector<int32_t>>& all_solutions , std::vector<int32_t>& current_solution , bool need_all )
{
    if ( this->dummy_column->get_right_ptr()->get_essential() == false )
    {
        // No more constraints left to be satisfied. Success
        all_solutions.push_back( current_solution );
        return true;
    }

    // Find the essential column with the lowest degree
    DancingLinksColumn * chosen_column = this->dummy_column->get_right_ptr();
    std::size_t min_count = chosen_column->get_size();
    for ( DancingLinksColumn * ptr = chosen_column->get_right_ptr() ; ptr->get_essential() ; ptr = ptr->get_right_ptr() )
    {
        if ( ptr->get_size() < min_count )
        {
            chosen_column = ptr;
            min_count = ptr->get_size();
        }
    }

    //no way to satisfy some constraint. failure
    if ( min_count == 0 )
        return false;
    bool flag = false;

    //remove the chosen column
    chosen_column->remove_column();
    for ( DancingLinksCell * ptr = chosen_column->get_dummy_cell()->get_down_ptr() ; ptr != chosen_column->get_dummy_cell() ; ptr = ptr->get_down_ptr() )
    {
        //pick this row in candidate solution

        //remove columns for all other cells in this row
        current_solution.push_back( ptr->get_row_id() );
        for ( DancingLinksCell * ptr2 = ptr->get_right_ptr() ; ptr2 != ptr ; ptr2 = ptr2->get_right_ptr() )
        {
            ptr2->get_column_ptr()->remove_column();
        }

        //recurse to solve the modified board
        if ( this->solve( all_solutions , current_solution , need_all ) )
            flag = true;

        //unremove all those columns
        for ( DancingLinksCell * ptr2 = ptr->get_right_ptr() ; ptr2 != ptr ; ptr2 = ptr2->get_right_ptr() )
        {
            ptr2->get_column_ptr()->unremove_column();
        }
        current_solution.pop_back();

        //if only one solution needs to be found and we have found one, don't bother checking for the rest
        if ( ( flag == true ) && ( need_all == false ) )
        {
            chosen_column->unremove_column();
            return flag;
        }
    }
    //unremove the chosen column
    chosen_column->unremove_column();
    return flag;
}
