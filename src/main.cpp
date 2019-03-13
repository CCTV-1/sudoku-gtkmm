#include <cstdint>

#include <array>
#include <exception>
#include <deque>
#include <string>
#include <utility>

#include <sigc++/sigc++.h>

#include <gtkmm/application.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/popover.h>
#include <gtkmm/window.h>

#include "sudoku.h"

constexpr SUDOKU_LEVEL NEW_GAME_LEVEL = SUDOKU_LEVEL::MEDIUM;

static const Gdk::RGBA BOARD_BACKGROUD_RBGA( Glib::ustring( "rgba( 255 , 255 , 255 , 0.8 )" ) );
static const Gdk::RGBA PUZZLE_NUMBER_RBGA  ( Glib::ustring( "rgba(   0 ,   0 ,   0 , 1.0 )" ) );
static const Gdk::RGBA SELECT_ROW_RBGA     ( Glib::ustring( "rgba( 226 , 231 , 237 , 1.0 )" ) );
static const Gdk::RGBA SELECT_CELL_RBGA    ( Glib::ustring( "rgba( 187 , 222 , 251 , 1.0 )" ) );
static const Gdk::RGBA SELECT_NUMBER_RBGA  ( Glib::ustring( "rgba(   1 ,   2 , 255 , 1.0 )" ) );

static const Gdk::RGBA BUTTON_BORDER_RGBA  ( Glib::ustring( "rgba( 190 , 198 , 212 , 1.0 )" ) );

static inline void set_rgba( const Cairo::RefPtr<Cairo::Context> & cairo_context , const Gdk::RGBA& rgba )
{
    cairo_context->set_source_rgba( rgba.get_red() , rgba.get_green() , rgba.get_blue() , rgba.get_alpha() );
}

/* class PuzzleCache
{
    public:
        PuzzleCache()
        {
            ;
        }
        ~PuzzleCache() = default;
        
        save_puzzle( SUDOKU_LEVEL  )
    private:
        std::array< std::mutex , SUDOKU_LEVEL::_LEVEL_COUNT > mutexs;
        std::array< std::vector<puzzle_t> , SUDOKU_LEVEL > cache;
} */

class SudokuBoard : public Gtk::DrawingArea
{
    public:
        enum class FillMode:std::uint32_t
        {
            FILL_SOLUTION = 0,
            FILL_CANDIDATE
        };

        SudokuBoard( Sudoku& game ):
            game( game ),
            puzzle( game.get_puzzle() ),
            candidates( game.get_candidates() ),
            candidate_font( "Ubuntu Mono 14" ),
            solutions_font( "Ubuntu Mono 28" )
        {

            /*
            * column_index_size: 5*$(font size)
            * '  A  '
            * 
            * grid size: 6*$(font size)
            *    '       '
            *    ' 1 2 3 '
            *    '       '
            *    ' 4 5 6 '
            *    '       '
            *    ' 7 8 9 '
            *    '       '
            *  board size: column_index_size*2 + 9*grid_size
            */

            add_events( Gdk::EventMask::BUTTON_PRESS_MASK );
            this->layout = this->create_pango_layout( "strings" );
            this->layout->set_font_description( this->candidate_font );
            this->font_size = this->candidate_font.get_size()/Pango::SCALE;
            this->bold_line_size = this->font_size/2;
            this->line_size = this->bold_line_size/2;
            this->grid_size = 6*( this->font_size );
            this->row_index_size = 5*this->font_size;
            this->column_index_size = 5*this->font_size;
            this->board_size = 2*( this->column_index_size ) + 9*( this->grid_size );
            this->set_size_request( board_size , board_size );
        }
        ~SudokuBoard()
        {
            ;
        }

        void new_game( SUDOKU_LEVEL level )
        {
            try
            {
                Sudoku new_sudoku( level );
                this->game = new_sudoku;
                this->operator_queues.clear();
                this->select_cell = false;
                this->grid_x = 0;
                this->grid_y = 0;
                this->queue_draw();
            }
            catch( const std::exception& e )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s" , e.what() );
            }
        }

        FillMode change_fill_mode()
        {
            if ( this->fill_mode == FillMode::FILL_SOLUTION )
            {
                this->fill_mode = FillMode::FILL_CANDIDATE;
            }
            else
            {
                this->fill_mode = FillMode::FILL_SOLUTION;
            }

            return this->fill_mode;
        }

        void fill_number( cell_t value )
        {
            if ( this->select_cell == false )
                return ;

            if ( this->fill_mode == FillMode::FILL_SOLUTION )
            {
                try
                {
                    this->game.fill_cell( this->grid_x , this->grid_y , value );
                    this->operator_queues.push_front( { OperatorType::FILL_PUZZLE , { this->grid_x , this->grid_y , value } } );
                }
                catch( const std::exception& e )
                {
                    g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s" , e.what() );
                }
            }
            else if ( this->fill_mode == FillMode::FILL_CANDIDATE )
            {
                auto const & candidates = this->game.get_candidates()[this->grid_x][this->grid_y];
                if ( std::find( candidates.begin() , candidates.end() , value ) == candidates.end() )
                {
                    this->game.fill_candidates( this->grid_x , this->grid_y , { value } );
                    this->operator_queues.push_front( { OperatorType::FILL_CANDIDATE , { this->grid_x , this->grid_y , value } } );
                }
                else
                {
                    this->game.erase_candidates( this->grid_x , this->grid_y , { value } );
                    this->operator_queues.push_front( { OperatorType::ERASE_CANDIDATE , { this->grid_x , this->grid_y , value } } );
                }
            }

            this->queue_draw();
        }

        void undo_operator( void )
        {
            if ( this->operator_queues.empty() )
                return ;
            auto operator_queue = this->operator_queues.front();
            switch ( operator_queue.first )
            {
                case OperatorType::FILL_PUZZLE:
                    this->game.erase_cell( operator_queue.second.x , operator_queue.second.y );
                    break;
                case OperatorType::FILL_CANDIDATE:
                    this->game.erase_candidates( operator_queue.second.x , operator_queue.second.y , { operator_queue.second.value } );
                    break;
                case OperatorType::ERASE_PUZZLE:
                    this->game.fill_cell( operator_queue.second.x , operator_queue.second.y , operator_queue.second.value );
                    break;
                case OperatorType::ERASE_CANDIDATE:
                    this->game.fill_candidates( operator_queue.second.x , operator_queue.second.y , { operator_queue.second.value } );
                    break;
                default:
                    break;
            }
            this->operator_queues.pop_front();
            this->queue_draw();
        }
    protected:
        bool on_draw( const Cairo::RefPtr<Cairo::Context> & cairo_context ) override
        {
            cairo_context->save();
            cairo_context->set_line_join( Cairo::LINE_JOIN_MITER );

            //draw background color
            //cairo_context->set_source_rgba( 1.0 , 1.0 , 1.0 , 0.8 );
            set_rgba( cairo_context , BOARD_BACKGROUD_RBGA );
            cairo_context->rectangle( 0 , 0 , this->board_size , this->board_size );
            cairo_context->stroke_preserve();
            cairo_context->fill();

            constexpr const char * row_indexs[] = 
            {
                "A" , "B" , "C" , "D" , "E" , "F" , "G" , "H" , "I"
            };
            constexpr const char * column_indexs[] = 
            {
                "1" , "2" , "3" , "4" , "5" , "6" , "7" , "8" , "9"
            };

            //draw index
            set_rgba( cairo_context , PUZZLE_NUMBER_RBGA );
            for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
            {
                this->layout->set_text( row_indexs[i] );
                cairo_context->move_to( this->row_index_size/2 , this->column_index_size + ( this->grid_size )/2 - ( this->font_size )/2 + i*( ( this->grid_size ) ) );
                this->layout->show_in_cairo_context( cairo_context );
                this->layout->set_text( column_indexs[i] );
                cairo_context->move_to( this->row_index_size + ( this->grid_size )/2 - ( this->font_size )/2 + i*( ( this->grid_size ) ) , this->column_index_size/2 );
                this->layout->show_in_cairo_context( cairo_context );
            }

            set_rgba( cairo_context , PUZZLE_NUMBER_RBGA );
            cairo_context->set_line_width( this->line_size );
            //draw board box
            cairo_context->rectangle( this->row_index_size , this->column_index_size , SUDOKU_SIZE*( this->grid_size ) , SUDOKU_SIZE*( this->grid_size ) );
            cairo_context->stroke();

            //draw select highlight
            if ( this->select_cell )
            {
                cell_t column_id = this->grid_x;
                cell_t row_id = this->grid_y;
                cell_t box_id = ( this->grid_x/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + this->grid_y/SUDOKU_BOX_SIZE;

                set_rgba( cairo_context , SELECT_ROW_RBGA );
                cairo_context->rectangle( this->row_index_size + this->line_size + row_id*( this->grid_size ) , this->column_index_size + this->line_size , 
                                ( this->grid_size ) - this->line_size , SUDOKU_SIZE*( this->grid_size ) - 2*this->line_size );
                cairo_context->stroke_preserve();
                cairo_context->fill();

                cairo_context->rectangle( this->row_index_size + this->line_size , this->column_index_size + this->line_size + column_id*( this->grid_size ) , 
                                SUDOKU_SIZE*( this->grid_size ) - 2*this->line_size , ( this->grid_size ) - 2*this->line_size );
                cairo_context->stroke_preserve();
                cairo_context->fill();

                cairo_context->rectangle( this->row_index_size + this->line_size + box_id%SUDOKU_BOX_SIZE*SUDOKU_BOX_SIZE*( this->grid_size ) , 
                                this->column_index_size + this->line_size + box_id/SUDOKU_BOX_SIZE*SUDOKU_BOX_SIZE*( this->grid_size ) , 
                                SUDOKU_BOX_SIZE*( this->grid_size ) - 2*this->line_size , SUDOKU_BOX_SIZE*( this->grid_size ) - 2*this->line_size );
                cairo_context->stroke_preserve();
                cairo_context->fill();

                set_rgba( cairo_context , SELECT_CELL_RBGA );
                cairo_context->rectangle( this->column_index_size + this->line_size + row_id*( this->grid_size ) , 
                                    this->row_index_size + this->line_size + column_id*( this->grid_size ) ,
                                    ( this->grid_size ) - 2*this->line_size , ( this->grid_size ) - 2*this->line_size );
                cairo_context->stroke_preserve();
                cairo_context->fill();
            }

            //draw board line
            set_rgba( cairo_context , PUZZLE_NUMBER_RBGA );
            for ( cell_t i = 1 ; i < SUDOKU_SIZE ; i++ )
            {
                if ( i%SUDOKU_BOX_SIZE == 0 )
                    cairo_context->set_line_width( this->bold_line_size );
                else
                    cairo_context->set_line_width( this->line_size );
                cairo_context->move_to( this->row_index_size + i*( this->grid_size ) , this->column_index_size );
                cairo_context->line_to( this->row_index_size + i*( this->grid_size ) , this->column_index_size + 
                                        SUDOKU_SIZE*( this->grid_size ) );

                cairo_context->move_to( this->row_index_size , this->column_index_size + i*( this->grid_size ) );
                cairo_context->line_to( this->row_index_size + SUDOKU_SIZE*( this->grid_size )
                                        , this->column_index_size + i*( this->grid_size ) );
                cairo_context->stroke();
            }

            //draw puzzle
            this->layout->set_font_description( solutions_font );
            auto size = this->solutions_font.get_size()/Pango::SCALE;
            for ( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
            {
                for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
                {
                    cell_t number = this->puzzle[i][j];
                    if ( number != 0 )
                    {
                        if ( this->select_cell && number == this->puzzle[this->grid_x][this->grid_y] )
                        {
                            set_rgba( cairo_context , SELECT_NUMBER_RBGA );
                        }
                        else
                        {
                            set_rgba( cairo_context , PUZZLE_NUMBER_RBGA );
                        }
                        this->layout->set_text( std::to_string( number ) );
                        cairo_context->move_to( this->row_index_size + j*( this->grid_size ) + ( this->grid_size )/2 - size/2
                                                , this->column_index_size + i*( this->grid_size ) + ( this->grid_size )/2 - size );
                        this->layout->show_in_cairo_context( cairo_context );
                    }
                }
            }

            //draw candiates
            set_rgba( cairo_context , PUZZLE_NUMBER_RBGA );
            this->layout->set_font_description( candidate_font );
            for ( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
            {
                for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
                {
                    auto number = puzzle[i][j];
                    if ( number == 0 )
                    {
                        for ( cell_t k = 0 ; k < SUDOKU_SIZE ; k++ )
                        {
                            if ( std::find( this->candidates[i][j].begin() , this->candidates[i][j].end() , k + 1 ) != this->candidates[i][j].end() )
                            {
                                this->layout->set_text( std::to_string( k + 1 ) );
                                cairo_context->move_to( this->row_index_size + j*( this->grid_size ) + ( this->font_size )/2 + k%SUDOKU_BOX_SIZE*2*( this->font_size )
                                            , this->column_index_size + i*( this->grid_size ) + k/SUDOKU_BOX_SIZE*2*( this->font_size ) );
                                this->layout->show_in_cairo_context( cairo_context );
                            }
                        }
                    }
                }
            }

            cairo_context->restore();
            return true;
        }
        bool on_button_press_event( GdkEventButton * event ) override
        {
            //ignore double-clicked and three-clicked 
            if ( ( event->type == GDK_2BUTTON_PRESS ) || ( event->type == GDK_3BUTTON_PRESS ) )
                return true;
            if ( ( event->x < this->column_index_size ) || ( event->x > this->column_index_size + 9*( this->grid_size ) ) )
                return true;
            if ( ( event->y < this->row_index_size ) || ( event->y > this->row_index_size + 9*( this->grid_size ) ) )
                return true;
            std::size_t x = event->x;
            std::size_t y = event->y;
            this->grid_x = ( y - this->row_index_size )/this->grid_size;
            this->grid_y = ( x - this->column_index_size )/this->grid_size;
            this->select_cell = true;

            this->queue_draw();
            return true;
        }
    private:
        //game inteface
        Sudoku& game;
        const puzzle_t& puzzle;
        const candidate_t& candidates;

        //ui desc
        Pango::FontDescription candidate_font;
        Pango::FontDescription solutions_font;
        Glib::RefPtr<Pango::Layout> layout;
        std::size_t bold_line_size;
        std::size_t line_size;
        std::size_t font_size;
        std::size_t row_index_size;
        std::size_t column_index_size;
        std::size_t grid_size;
        std::size_t board_size;

        //game status
        FillMode fill_mode = FillMode::FILL_SOLUTION;
        bool select_cell = false;
        cell_t grid_x = 0;
        cell_t grid_y = 0;

        enum class OperatorType:std::uint32_t
        {
            FILL_PUZZLE = 0,
            FILL_CANDIDATE,
            ERASE_PUZZLE,
            ERASE_CANDIDATE
        };
        struct OperatorValue
        {
            cell_t x;
            cell_t y;
            cell_t value;
        };

        typedef std::deque< std::pair< OperatorType , OperatorValue > > operator_t;
        operator_t operator_queues;
};

class ControlButton : public Gtk::EventBox
{
    public:
        ControlButton( const Glib::ustring& label = "" , const Glib::ustring& font_name = "Ubuntu Mono 28" ):
            label( label ),
            font_desc( font_name ),
            pointer_enters( false )
        {
            this->layout = this->create_pango_layout( label );
            this->layout->set_font_description( this->font_desc );
            std::size_t font_size = font_desc.get_size()/Pango::SCALE;
            this->set_size_request( font_size*4 , font_size*4 );
            this->set_events( Gdk::EventMask::BUTTON_PRESS_MASK |
                              Gdk::EventMask::LEAVE_NOTIFY_MASK |
                              Gdk::EventMask::ENTER_NOTIFY_MASK
            );
        }
        ~ControlButton()
        {
            ;
        }

        void set_label( const Glib::ustring& label )
        {
            this->label = label;
            this->queue_draw();
        }

        void set_font( const Glib::ustring& font_name )
        {
            this->font_desc = Pango::FontDescription( font_name );
            this->layout->set_font_description( this->font_desc );
            std::size_t font_size = font_desc.get_size()/Pango::SCALE;
            this->set_size_request( font_size*4 , font_size*4 );
        }
    protected:
        bool on_draw( const Cairo::RefPtr<Cairo::Context> & cairo_context ) override
        {
            cairo_context->save();

            //draw background color
            if ( pointer_enters )
                set_rgba( cairo_context , SELECT_NUMBER_RBGA );
            else
                set_rgba( cairo_context , BOARD_BACKGROUD_RBGA );
            cairo_context->rectangle( 0 , 0 , this->get_allocated_width() , this->get_allocated_height() );
            cairo_context->stroke_preserve();
            cairo_context->fill();

            set_rgba( cairo_context , BUTTON_BORDER_RGBA );
            cairo_context->rectangle( 0 , 0 , this->get_allocated_width() , this->get_allocated_height() );
            cairo_context->stroke();
            cairo_context->fill();

            set_rgba( cairo_context , PUZZLE_NUMBER_RBGA );
            this->layout->set_text( this->label );
            int layout_width = 0 , layout_height = 0;
            this->layout->get_pixel_size( layout_width , layout_height );
            cairo_context->move_to( ( this->get_allocated_width() - layout_width )/2 ,
                                    ( this->get_allocated_height() - layout_height )/2 );
            this->layout->show_in_cairo_context( cairo_context );

            cairo_context->restore();
            return true;
        }

        bool on_enter_notify_event( GdkEventCrossing * ) override
        {
            this->pointer_enters = true;
            this->queue_draw();
            return true;
        }

        bool on_leave_notify_event( GdkEventCrossing * ) override
        {
            this->pointer_enters = false;
            this->queue_draw();
            return true;
        }
    private:
        Glib::ustring label;
        Glib::RefPtr<Pango::Layout> layout;
        Pango::FontDescription font_desc;

        bool pointer_enters;
};

bool fill_number( GdkEventButton * event , SudokuBoard& board , cell_t value )
{
    //ignore double-clicked and three-clicked 
    if ( ( event->type == GDK_2BUTTON_PRESS ) || ( event->type == GDK_3BUTTON_PRESS ) )
        return true;

    board.fill_number( value );

    return true;
}

bool set_fill_mode( GdkEventButton * event , SudokuBoard& board , ControlButton& button )
{
    //ignore double-clicked and three-clicked 
    if ( ( event->type == GDK_2BUTTON_PRESS ) || ( event->type == GDK_3BUTTON_PRESS ) )
        return true;

    SudokuBoard::FillMode new_mode = board.change_fill_mode();

    switch ( new_mode )
    {
        case SudokuBoard::FillMode::FILL_SOLUTION:
            button.set_label( "Fill Mode:Solution" );
            break;
        case SudokuBoard::FillMode::FILL_CANDIDATE:
            button.set_label( "Fill Mode:Candidate" );
            break;
        default:
            break;
    }

    return true;
}

bool new_game( GdkEventButton * event , SudokuBoard& board , SUDOKU_LEVEL level )
{
    //ignore double-clicked and three-clicked 
    if ( ( event->type == GDK_2BUTTON_PRESS ) || ( event->type == GDK_3BUTTON_PRESS ) )
        return true;

    board.new_game( level );

    return true;
}

bool open_game_menu( GdkEventButton * event , Gtk::Popover& level_menu )
{
    //ignore double-clicked and three-clicked 
    if ( ( event->type == GDK_2BUTTON_PRESS ) || ( event->type == GDK_3BUTTON_PRESS ) )
        return true;

    Gtk::Widget * relative_widget = level_menu.get_relative_to();
    level_menu.set_size_request( relative_widget->get_allocated_width() );
    level_menu.set_pointing_to( { relative_widget->get_allocated_width()/2 , relative_widget->get_allocated_height()/2 , 0 , 0 } );
    level_menu.popup();

    return true;
}

bool undo_operator( GdkEventButton * event , SudokuBoard& board )
{
    //ignore double-clicked and three-clicked 
    if ( ( event->type == GDK_2BUTTON_PRESS ) || ( event->type == GDK_3BUTTON_PRESS ) )
        return true;
    
    board.undo_operator();

    return true;
}

int main( void )
{
    Sudoku sudoku( NEW_GAME_LEVEL );

    auto app = Gtk::Application::create();
    Gtk::Window window;
    window.set_resizable( false );

    Gtk::Grid main_grid;
    main_grid.set_row_homogeneous();
    main_grid.set_column_homogeneous( true );

    SudokuBoard sudoku_board( std::ref( sudoku ) );
    main_grid.attach( sudoku_board , 0 , 0 , 12 , 12 );

    Gtk::Grid popover_grid;
    popover_grid.set_row_homogeneous();
    popover_grid.set_column_homogeneous( true );
    std::array< ControlButton , static_cast<std::size_t>( SUDOKU_LEVEL::_LEVEL_COUNT ) > level_button_arr;
    for( cell_t i = 0 ; i < static_cast<cell_t>( SUDOKU_LEVEL::_LEVEL_COUNT ) ; i++ )
    {
        level_button_arr[i].set_label( dump_level( static_cast<SUDOKU_LEVEL>( i ) ) );
        level_button_arr[i].set_font( "Ubuntu Mono 14" );
        level_button_arr[i].signal_button_press_event().connect( sigc::bind( &new_game , std::ref( sudoku_board ) , static_cast<SUDOKU_LEVEL>( i )  ) );
        popover_grid.attach( level_button_arr[i] , 0 , i );
    }
    popover_grid.show_all();

    ControlButton new_game_button( "New Game" , "Ubuntu Mono 28" );
    Gtk::Popover level_menu;
    level_menu.add( popover_grid );
    level_menu.set_relative_to( new_game_button );
    new_game_button.signal_button_press_event().connect( sigc::bind( &open_game_menu , std::ref( level_menu ) ) );
    main_grid.attach( new_game_button , 12 , 0 , 6 , 3 );

    std::array< ControlButton , SUDOKU_SIZE > button_arr;
    for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        button_arr[i].set_label( std::to_string( i + 1 ) );
        button_arr[i].signal_button_press_event().connect( sigc::bind( &fill_number , std::ref( sudoku_board ) , i + 1 ) );
        main_grid.attach( button_arr[i] , 12 + i%SUDOKU_BOX_SIZE*2 , 3 + i/SUDOKU_BOX_SIZE*2 , 2 , 2 );
    }

    ControlButton fill_mode_button( "Fill Mode:Solution" , "Ubuntu Mono 14" );
    fill_mode_button.signal_button_press_event().connect( sigc::bind( &set_fill_mode , std::ref( sudoku_board ) , std::ref( fill_mode_button ) ) );
    main_grid.attach( fill_mode_button , 12 , 9 , 3 , 3 );

    ControlButton undo_button( "Undo" , "Ubuntu Mono 14" );
    undo_button.signal_button_press_event().connect( sigc::bind( &undo_operator , std::ref( sudoku_board ) ) );
    main_grid.attach( undo_button , 15 , 9 , 3 , 3 );

    window.add( main_grid );
    window.show_all();
    return app->run( window );
}
