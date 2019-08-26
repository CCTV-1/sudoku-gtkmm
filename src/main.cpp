#include <cstdint>

#include <array>
#include <chrono>
#include <deque>
#include <exception>
#include <filesystem>
#include <future>
#include <string>
#include <utility>

#include <sigc++/sigc++.h>
#include <glibmm/main.h>
#include <glibmm/timer.h>
#include <gtkmm/application.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/clipboard.h>
#include <gtkmm/button.h>
#include <gtkmm/builder.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/grid.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/popover.h>
#include <gtkmm/window.h>
#include <gtkmm/window.h>

#include "sudoku.h"

constexpr SUDOKU_LEVEL NEW_GAME_LEVEL = SUDOKU_LEVEL::MEDIUM;
static const Glib::ustring UI_FILE( "./resource/sudoku.ui" );

static Gdk::RGBA BOARD_BACKGROUD_RGBA( Glib::ustring( "rgba( 255 , 255 , 255 , 1.0 )" ) );
static Gdk::RGBA PUZZLE_NUMBER_RGBA  ( Glib::ustring( "rgba(   0 ,   0 ,   0 , 1.0 )" ) );
static Gdk::RGBA ANSWER_NUMBER_RGBA  ( Glib::ustring( "rgba(  74 , 144 , 226 , 1.0 )" ) );
static Gdk::RGBA SELECT_ROW_RGBA     ( Glib::ustring( "rgba( 226 , 231 , 237 , 1.0 )" ) );
static Gdk::RGBA SELECT_CELL_RGBA    ( Glib::ustring( "rgba( 187 , 222 , 251 , 1.0 )" ) );
static Gdk::RGBA SELECT_NUMBER_RGBA  ( Glib::ustring( "rgba(   1 ,   2 , 255 , 1.0 )" ) );
static Gdk::RGBA BUTTON_BORDER_RGBA  ( Glib::ustring( "rgba( 190 , 198 , 212 , 1.0 )" ) );

enum class Themes:std::uint32_t
{
    LIGHT,
    DARK
};

static void set_theme( const Themes theme )
{
    switch ( theme )
    {
        default:
        case Themes::LIGHT:
        {
            BOARD_BACKGROUD_RGBA.set( Glib::ustring( "rgba( 255 , 255 , 255 , 1.0 )" ) );
            PUZZLE_NUMBER_RGBA.set( Glib::ustring( "rgba(   0 ,   0 ,   0 , 1.0 )" ) );
            ANSWER_NUMBER_RGBA.set( Glib::ustring( "rgba(  74 , 144 , 226 , 1.0 )" ) );
            SELECT_ROW_RGBA.set( Glib::ustring( "rgba( 226 , 231 , 237 , 1.0 )" ) );
            SELECT_CELL_RGBA.set( Glib::ustring( "rgba( 187 , 222 , 251 , 1.0 )" ) );
            SELECT_NUMBER_RGBA.set( Glib::ustring( "rgba(   1 ,   2 , 255 , 1.0 )" ) );
            BUTTON_BORDER_RGBA.set( Glib::ustring( "rgba( 190 , 198 , 212 , 1.0 )" ) );
            break;
        }
        case Themes::DARK:
        {
            BOARD_BACKGROUD_RGBA.set( Glib::ustring( "rgba( 45 , 45 , 45 , 1.0 )" ) );
            PUZZLE_NUMBER_RGBA.set( Glib::ustring( "rgba( 171 , 171 , 171 , 1.0 )" ) );
            ANSWER_NUMBER_RGBA.set( Glib::ustring( "rgba( 255 , 255 , 255 , 1.0 )" ) );
            SELECT_ROW_RGBA.set( Glib::ustring( "rgba( 141 ,  38 ,  99 , 1.0 )" ) );
            SELECT_CELL_RGBA.set( Glib::ustring( "rgba(  86 , 187 , 235 , 1.0 )" ) );
            SELECT_NUMBER_RGBA.set( Glib::ustring( "rgba(  44 , 243 , 151 , 1.0 )" ) );
            BUTTON_BORDER_RGBA.set( Glib::ustring( "rgba( 241 , 224 ,  10 , 1.0 )" ) );
            break;
        }
    }
}

static void set_rgba( const Cairo::RefPtr<Cairo::Context> & cairo_context , const Gdk::RGBA& rgba )
{
    cairo_context->set_source_rgba( rgba.get_red() , rgba.get_green() , rgba.get_blue() , rgba.get_alpha() );
}

class SudokuBoard : public Gtk::DrawingArea
{
    public:
        enum class FillMode:std::uint32_t
        {
            ANSWER = 0,
            CANDIDATE
        };

        enum class GameState:std::uint32_t
        {
            PLAYING = 0,
            PAUSE,
            VIEW_SOLUTION,
            LOADING_NEW_GAME
        };

        enum class PrintType:std::uint32_t
        {
            PNG = 0,
            #ifdef CAIRO_HAS_PDF_SURFACE
                PDF,
            #endif
            #ifdef CAIRO_HAS_PS_SURFACE
                PS,
            #endif
            #ifdef CAIRO_HAS_SVG_SURFACE
                SVG,
            #endif
        };

        //Constructor( BaseObjectType * cobject , const Glib::RefPtr<Gtk::Builder>& ):
        //              BaseClass( cobject )
        //to support Gtk::Builder::get_widget_derived()
        SudokuBoard( BaseObjectType * cobject , const Glib::RefPtr<Gtk::Builder>& ):
            Gtk::DrawingArea( cobject ),
            game(),
            puzzle( game.get_puzzle() ),
            candidates( game.get_candidates() ),
            solution( game.get_solution(false)[0] ),
            candidate_font( "Ubuntu Mono 14" ),
            solutions_font( "Ubuntu Mono 28" ),
            state( GameState::PLAYING ),
            timer()
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

            this->add_events( Gdk::EventMask::BUTTON_PRESS_MASK );
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

            this->operator_queues = { {} };
            //undo operator sentinel
            this->operator_iterator = this->operator_queues.begin();
        }
        ~SudokuBoard()
        {
            ;
        }

        //network puzzle
        void new_game( SUDOKU_LEVEL level )
        {
            this->set_game_state( SudokuBoard::GameState::LOADING_NEW_GAME );
            std::shared_future <puzzle_t> puzzle_future = get_network_puzzle( level );
            Glib::signal_timeout().connect(
                [ this , puzzle_future , level ]()
                {
                    std::future_status puzzle_status;
                    puzzle_status = puzzle_future.wait_for( std::chrono::microseconds( 20 ) );
                    if ( puzzle_status == std::future_status::ready )
                    {
                        try
                        {
                            Sudoku new_sudoku( puzzle_future.get() , level );
                            this->game = new_sudoku;
                        }
                        catch( const std::exception& e )
                        {
                            g_log( __func__ , G_LOG_LEVEL_MESSAGE , e.what() );
                            //rollback to old game
                            this->set_game_state( SudokuBoard::GameState::PLAYING );
                            return false;
                        }
                        auto auto_answer = game.get_solution( false );
                        if ( auto_answer.empty() )
                        {
                            return false;
                        }
                        this->solution = auto_answer[0];
                        this->operator_queues = { {} };
                        this->operator_iterator = this->operator_queues.begin();
                        this->select_cell = false;
                        this->grid_x = 0;
                        this->grid_y = 0;
                        this->prev_time = 0;
                        this->timer.reset();
                        this->set_game_state( GameState::PLAYING );
                        this->queue_draw();

                        return false;
                    }

                    return true;
                },
                200
            );
        }

        //local generate puzzle
        void new_game()
        {
            try
            {
                this->game = Sudoku();
            }
            catch( const std::exception& e )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , e.what() );
                //rollback to old game
                this->set_game_state( SudokuBoard::GameState::PLAYING );
                return ;
            }
            auto auto_answer = game.get_solution( false );
            if ( auto_answer.empty() )
            {
                //back to empty puzzle
                this->game = Sudoku();
                auto_answer = game.get_solution( false );
                return ;
            }
            this->solution = auto_answer[0];
            this->operator_queues = { {} };
            this->operator_iterator = this->operator_queues.begin();
            this->select_cell = false;
            this->grid_x = 0;
            this->grid_y = 0;
            this->prev_time = 0;
            this->timer.reset();
            this->set_game_state( GameState::PLAYING );
            this->queue_draw();
        }

        void new_game( Glib::ustring puzzle_string )
        {
            try
            {
                Sudoku new_sudoku( serialization_puzzle( puzzle_string ) );
                this->game = new_sudoku;
            }
            catch( const std::exception& e )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , e.what() );
                return ;
            }
            this->solution = game.get_solution(false)[0];
            this->operator_queues = { {} };
            this->operator_iterator = this->operator_queues.begin();
            this->select_cell = false;
            this->grid_x = 0;
            this->grid_y = 0;
            this->prev_time = 0;
            this->timer.reset();
            this->set_game_state( GameState::PLAYING );
            this->queue_draw();
        }

        void auto_update_candidate( bool flags )
        {
            this->game.autoupdate_candidate( flags );
        }

        GameState get_game_state( void )
        {
            return this->state;
        }
        
        void set_game_state( GameState state )
        {
            if ( state == GameState::PLAYING )
            {
                //call start() reset timer,save time
                this->prev_time += this->timer.elapsed();
                this->timer.start();
            }
            else if ( state == GameState::PAUSE )
            {
                this->timer.stop();
            }

            this->state = state;
            this->queue_draw();
        }

        std::uint32_t get_play_time( void )
        {
            return this->prev_time + this->timer.elapsed();
        }

        FillMode get_fill_mode( void )
        {
            return this->fill_mode;
        }

        void set_fill_mode( FillMode mode )
        {
            this->fill_mode = mode;
        }

        void do_fill( cell_t value )
        {
            if ( this->select_cell == false )
                return ;
            if ( this->state != GameState::PLAYING )
                return ;

            //keep undo operator sentinel element
            this->operator_queues.resize( this->operator_iterator - this->operator_queues.begin() + 1 );

            switch ( this->fill_mode )
            {
                case FillMode::ANSWER:
                {
                    try
                    {
                        cell_t old_value = this->puzzle[this->grid_x][this->grid_y];
                        if ( value == old_value )
                        {
                            this->game.erase_answer( this->grid_x , this->grid_y );
                            this->operator_queues.push_back( { OperatorType::ERASE_ANSWER , { this->grid_x , this->grid_y , value } } );
                        }
                        else
                        {
                            this->game.fill_answer( this->grid_x , this->grid_y , value );
                            this->operator_queues.push_back( { OperatorType::FILL_ANSWER , { this->grid_x , this->grid_y , value } } );
                        }
                    }
                    catch( const std::exception& e )
                    {
                        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s" , e.what() );
                    }
                    break;
                }
                case FillMode::CANDIDATE:
                {
                    auto const & candidates = this->game.get_candidates()[this->grid_x][this->grid_y];
                    if ( std::find( candidates.begin() , candidates.end() , value ) == candidates.end() )
                    {
                        this->game.fill_candidates( this->grid_x , this->grid_y , { value } );
                        this->operator_queues.push_back( { OperatorType::FILL_CANDIDATE , { this->grid_x , this->grid_y , value } } );
                    }
                    else
                    {
                        this->game.erase_candidates( this->grid_x , this->grid_y , { value } );
                        this->operator_queues.push_back( { OperatorType::ERASE_CANDIDATE , { this->grid_x , this->grid_y , value } } );
                    }
                    break;
                }
                default:
                    break;
            }

            this->operator_iterator++;
            this->queue_draw();
        }

        void undo_fill( void )
        {
            if ( this->operator_queues.begin() == this->operator_iterator )
                return ;
            auto operator_queue = *( this->operator_iterator );
            switch ( operator_queue.first )
            {
                case OperatorType::FILL_ANSWER:
                {
                    if ( operator_queue.second.value != 0 )
                        this->game.erase_answer( operator_queue.second.x , operator_queue.second.y );
                    else
                        this->game.fill_answer( operator_queue.second.x , operator_queue.second.y , operator_queue.second.value );
                    break;
                }
                case OperatorType::FILL_CANDIDATE:
                    this->game.erase_candidates( operator_queue.second.x , operator_queue.second.y , { operator_queue.second.value } );
                    break;
                case OperatorType::ERASE_ANSWER:
                    this->game.fill_answer( operator_queue.second.x , operator_queue.second.y , operator_queue.second.value );
                    break;
                case OperatorType::ERASE_CANDIDATE:
                    this->game.fill_candidates( operator_queue.second.x , operator_queue.second.y , { operator_queue.second.value } );
                    break;
                default:
                    break;
            }
            
            this->operator_iterator--;
            this->queue_draw();
        }

        void redo_fill( void )
        {
            if ( this->operator_queues.end() == this->operator_iterator )
                return ;
            this->operator_iterator++;
            if ( this->operator_queues.end() == this->operator_iterator )
            {
                this->operator_iterator--;
                return ;
            }
            auto operator_queue = *( this->operator_iterator );
            switch ( operator_queue.first )
            {
                case OperatorType::FILL_ANSWER:
                {
                    if ( operator_queue.second.value != 0 )
                        this->game.fill_answer( operator_queue.second.x , operator_queue.second.y , operator_queue.second.value );
                    else
                        this->game.erase_answer( operator_queue.second.x , operator_queue.second.y );
                    break;
                }
                case OperatorType::FILL_CANDIDATE:
                    this->game.fill_candidates( operator_queue.second.x , operator_queue.second.y , { operator_queue.second.value } );
                    break;
                case OperatorType::ERASE_ANSWER:
                    this->game.erase_answer( operator_queue.second.x , operator_queue.second.y );
                    break;
                case OperatorType::ERASE_CANDIDATE:
                    this->game.erase_candidates( operator_queue.second.x , operator_queue.second.y , { operator_queue.second.value } );
                    break;
                default:
                    break;
            }

            this->queue_draw();
        }

        Glib::ustring dump_play_time( void )
        {
            std::uint32_t playing_time = this->get_play_time();
            Glib::ustring time_string( std::to_string( playing_time/3600 ) );
            time_string += ":" + std::to_string( playing_time/60 );
            time_string += ":" + std::to_string( playing_time%60 );
            return time_string;
        }

        bool can_fill( double x , double y )
        {
            if ( this->get_game_state() != GameState::PLAYING )
                return false;
            if ( ( x < this->column_index_size ) || ( x > this->column_index_size + 9*( this->grid_size ) ) )
                return false;
            if ( ( y < this->row_index_size ) || ( y > this->row_index_size + 9*( this->grid_size ) ) )
                return false;
            cell_t grid_x = ( y - this->row_index_size )/this->grid_size;
            cell_t grid_y = ( x - this->column_index_size )/this->grid_size;
            
            return ( ( this->puzzle[grid_x][grid_y] == 0 ) || ( this->game.is_answer( grid_x , grid_y ) ) );
        }

        Glib::ustring dump_sudoku( void )
        {
            return dump_puzzle( this->puzzle );
        }

        void print_puzzle( Glib::ustring filename = "puzzle.png" ,  PrintType type = PrintType::PNG )
        {
            auto allocation = this->get_allocation();
    
            Cairo::RefPtr<Cairo::Surface> surface;
            switch ( type )
            {
                case PrintType::PNG:
                {
                    surface = Cairo::ImageSurface::create( Cairo::FORMAT_ARGB32 , allocation.get_width() , allocation.get_height() );
                    break;
                }
                case PrintType::PDF:
                {
                    surface = Cairo::PdfSurface::create( filename , allocation.get_width() , allocation.get_height() );
                    break;
                }
                case PrintType::PS:
                {
                    surface = Cairo::PsSurface::create( filename , allocation.get_width() , allocation.get_height() );
                    break;
                }
                case PrintType::SVG:
                {
                    surface = Cairo::SvgSurface::create( filename , allocation.get_width() , allocation.get_height() );
                    break;
                }
                default:
                    return ;
            }

            auto cairo_context = Cairo::Context::create( surface );
            this->draw_background_color( cairo_context );
            this->draw_index( cairo_context );
            this->draw_board_box( cairo_context );
            this->draw_board_line( cairo_context );
            this->draw_puzzle( cairo_context , this->puzzle );

            //write file may throw exception,but does not affect other functions.
            //so just output warning and return.
            try
            {
                switch( type )
                {
                    case PrintType::PNG:
                        surface->write_to_png( filename );
                        break;
                    case PrintType::PDF:
                    case PrintType::PS:
                    case PrintType::SVG:
                        surface->show_page();
                        break;
                }
            }
            catch( const std::exception& e )
            {
                g_log( __func__ , G_LOG_LEVEL_WARNING , "writing file:'%s' failure,exception message:'%s'"
                    , filename.c_str() , e.what() );
            }
        }

    protected:
        bool on_draw( const Cairo::RefPtr<Cairo::Context> & cairo_context ) override
        {
            cairo_context->save();
            cairo_context->set_line_join( Cairo::LINE_JOIN_MITER );

            this->draw_background_color( cairo_context );

            auto state_to_string = [ this ]() -> std::string
            {
                switch ( this->state )
                {
                    case GameState::PAUSE:
                        return std::string( "game pause,disable board view" );
                    case GameState::LOADING_NEW_GAME:
                        return std::string( "loading puzzle,not available board view" );
                    //playing,view solution don't need to string
                    default:
                        return std::string( "unknow state" );
                }
            };
            if ( ( this->state == GameState::PAUSE ) || ( this->state == GameState::LOADING_NEW_GAME ) )
            {
                set_rgba( cairo_context , PUZZLE_NUMBER_RGBA );
                this->layout->set_text( state_to_string() );
                int layout_width = 0 , layout_height = 0;
                this->layout->get_pixel_size( layout_width , layout_height );
                cairo_context->move_to( this->board_size/2 - layout_width/2 , this->board_size/2 - layout_height/2 );
                this->layout->show_in_cairo_context( cairo_context );
                cairo_context->restore();
                return true;
            }

            this->draw_index( cairo_context );
            this->draw_board_box( cairo_context );

            //draw select highlight
            if ( this->select_cell )
            {
                cell_t column_id = this->grid_x;
                cell_t row_id = this->grid_y;
                cell_t box_id = ( this->grid_x/SUDOKU_BOX_SIZE )*SUDOKU_BOX_SIZE + this->grid_y/SUDOKU_BOX_SIZE;

                set_rgba( cairo_context , SELECT_ROW_RGBA );
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

                set_rgba( cairo_context , SELECT_CELL_RGBA );
                cairo_context->rectangle( this->column_index_size + this->line_size + row_id*( this->grid_size ) , 
                                    this->row_index_size + this->line_size + column_id*( this->grid_size ) ,
                                    ( this->grid_size ) - 2*this->line_size , ( this->grid_size ) - 2*this->line_size );
                cairo_context->stroke_preserve();
                cairo_context->fill();
            }

            this->draw_board_line( cairo_context );

            if ( this->state == GameState::VIEW_SOLUTION )
            {
                this->draw_puzzle( cairo_context , this->solution );
                //restore on_draw save
                cairo_context->restore();
                return true;
            }

            //draw puzzle
            this->layout->set_font_description( solutions_font );
            //auto size = this->solutions_font.get_size()/Pango::SCALE;
            for ( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
            {
                for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
                {
                    cell_t number = this->puzzle[i][j];
                    if ( number != 0 )
                    {
                        if ( this->select_cell && number == this->puzzle[this->grid_x][this->grid_y] )
                        {
                            set_rgba( cairo_context , SELECT_NUMBER_RGBA );
                        }
                        else if ( this->game.is_answer( i , j ) )
                        {
                            set_rgba( cairo_context , ANSWER_NUMBER_RGBA );
                        }
                        else
                        {
                            set_rgba( cairo_context , PUZZLE_NUMBER_RGBA );
                        }
                        this->layout->set_text( std::to_string( number ) );
                        int layout_width , layout_height;
                        this->layout->get_pixel_size( layout_width , layout_height );
                        cairo_context->move_to( this->row_index_size + j*( this->grid_size ) + ( this->grid_size )/2 - layout_width/2
                                                , this->column_index_size + i*( this->grid_size ) + ( this->grid_size )/2 - layout_height/2 );
                        this->layout->show_in_cairo_context( cairo_context );
                    }
                }
            }

            //draw candiates
            set_rgba( cairo_context , PUZZLE_NUMBER_RGBA );
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
            //other game state disable button event
            if ( ( this->state != GameState::PLAYING ) && ( this->state != GameState::VIEW_SOLUTION ) )
                return false;
            //ignore double-clicked and three-clicked(left and mid button)
            if ( event->type == GdkEventType::GDK_2BUTTON_PRESS )
                return false;
            if ( event->type == GdkEventType::GDK_3BUTTON_PRESS )
                return false;
            if ( ( event->x < this->column_index_size ) || ( event->x > this->column_index_size + 9*( this->grid_size ) ) )
                return false;
            if ( ( event->y < this->row_index_size ) || ( event->y > this->row_index_size + 9*( this->grid_size ) ) )
                return false;
            std::size_t x = event->x;
            std::size_t y = event->y;
            this->grid_x = ( y - this->row_index_size )/this->grid_size;
            this->grid_y = ( x - this->column_index_size )/this->grid_size;
            this->select_cell = true;
            this->queue_draw();

            return false;
        }

    private:
        //game inteface
        Sudoku game;
        const puzzle_t& puzzle;
        const candidate_t& candidates;
        puzzle_t solution;

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
        FillMode fill_mode = FillMode::ANSWER;
        bool select_cell = false;
        cell_t grid_x = 0;
        cell_t grid_y = 0;

        GameState state;
        Glib::Timer timer;
        //Glib::Timer::stop() -> Glib::Timer::start() reset the timer,not exist Glib::Timer::resume()
        //save prev timer time to support resume
        double prev_time = 0;

        enum class OperatorType:std::uint32_t
        {
            FILL_ANSWER = 0,
            FILL_CANDIDATE,
            ERASE_ANSWER,
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
        operator_t::iterator operator_iterator;

        void draw_background_color( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            cairo_context->save();
            //draw background color
            set_rgba( cairo_context , BOARD_BACKGROUD_RGBA );
            cairo_context->paint();
            //cairo_context->rectangle( 0 , 0 , this->board_size , this->board_size );
            //cairo_context->stroke_preserve();
            //cairo_context->fill();
            cairo_context->restore();
        }

        void draw_index( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            cairo_context->save();
            constexpr const char * row_indexs[] = 
            {
                "A" , "B" , "C" , "D" , "E" , "F" , "G" , "H" , "I"
            };
            constexpr const char * column_indexs[] = 
            {
                "1" , "2" , "3" , "4" , "5" , "6" , "7" , "8" , "9"
            };

            //draw index
            set_rgba( cairo_context , PUZZLE_NUMBER_RGBA );
            this->layout->set_font_description( this->candidate_font );
            cairo_context->set_line_width( this->line_size );
            for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
            {
                int layout_width , layout_height;
                this->layout->set_text( row_indexs[i] );
                this->layout->get_pixel_size( layout_width , layout_height );
                cairo_context->move_to( this->row_index_size/2 - layout_width/2 , 
                    this->column_index_size + ( this->grid_size )/2 - ( this->font_size )/2 + i*( ( this->grid_size ) ) - layout_height/2 );
                this->layout->show_in_cairo_context( cairo_context );
                this->layout->set_text( column_indexs[i] );
                this->layout->get_pixel_size( layout_width , layout_height );
                cairo_context->move_to( this->row_index_size + ( this->grid_size )/2 - ( this->font_size )/2 + i*( ( this->grid_size ) ) - layout_width/2 ,
                    this->column_index_size/2 - layout_height/2 );
                this->layout->show_in_cairo_context( cairo_context );
            }
            cairo_context->restore();
        }

        void draw_board_box( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            cairo_context->save();
            //draw board box
            set_rgba( cairo_context , PUZZLE_NUMBER_RGBA );
            cairo_context->rectangle( this->row_index_size , this->column_index_size , SUDOKU_SIZE*( this->grid_size ) , SUDOKU_SIZE*( this->grid_size ) );
            cairo_context->stroke();
            cairo_context->restore();
        }

        void draw_board_line( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            cairo_context->save();
            //draw board line
            set_rgba( cairo_context , PUZZLE_NUMBER_RGBA );
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
            cairo_context->restore();
        }

        void draw_puzzle( const Cairo::RefPtr<Cairo::Context> & cairo_context , const puzzle_t& puzzle )
        {
            cairo_context->save();
            //draw puzzle
            set_rgba( cairo_context , PUZZLE_NUMBER_RGBA );
            this->layout->set_font_description( solutions_font );
            for ( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
            {
                for ( cell_t j = 0 ; j < SUDOKU_SIZE ; j++ )
                {
                    cell_t number = puzzle[i][j];
                    if ( number == 0 )
                        continue;
                    this->layout->set_text( std::to_string( number ) );
                    int layout_width , layout_height;
                    this->layout->get_pixel_size( layout_width , layout_height );
                    cairo_context->move_to( this->row_index_size + j*( this->grid_size ) + ( this->grid_size )/2 - layout_width/2
                                                , this->column_index_size + i*( this->grid_size ) + ( this->grid_size )/2 - layout_height/2 );
                    this->layout->show_in_cairo_context( cairo_context );
                }
            }

            cairo_context->restore();
        }

};

class ControlButton : public Gtk::EventBox
{
    public:
        ControlButton( const Glib::ustring& label = "" , const Glib::ustring& font_name = "Ubuntu Mono 28" )
            :label( label ),
            font_desc( font_name ),
            pointer_enters( false )
        {
            this->layout = this->create_pango_layout( label );
            this->layout->set_font_description( this->font_desc );
            std::size_t font_size = font_desc.get_size()/Pango::SCALE;
            this->layout->set_text( label );
            int width = 0 , height = 0;
            this->layout->get_pixel_size( width , height );
            this->set_size_request( width + 2*font_size , height + 2*font_size );
            this->set_events( Gdk::EventMask::BUTTON_PRESS_MASK |
                              Gdk::EventMask::LEAVE_NOTIFY_MASK |
                              Gdk::EventMask::ENTER_NOTIFY_MASK
            );
        }
        ControlButton( BaseObjectType * cobject , const Glib::RefPtr<Gtk::Builder>& , 
            const Glib::ustring& label = "" , const Glib::ustring& font_name = "Ubuntu Mono 28" )
            :Gtk::EventBox( cobject ),
            label( label ),
            font_desc( font_name ),
            pointer_enters( false )
        {
            this->layout = this->create_pango_layout( label );
            this->layout->set_font_description( this->font_desc );
            std::size_t font_size = font_desc.get_size()/Pango::SCALE;
            this->layout->set_text( label );
            int width = 0 , height = 0;
            this->layout->get_pixel_size( width , height );
            this->set_size_request( width + 2*font_size , height + 2*font_size );
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
            std::size_t font_size = font_desc.get_size()/Pango::SCALE;
            this->layout->set_text( label );
            int width = 0 , height = 0;
            this->layout->get_pixel_size( width , height );
            this->set_size_request( width + 2*font_size , height + 2*font_size );
            this->queue_resize_no_redraw();
        }

        void set_font( const Glib::ustring& font_name )
        {
            this->font_desc = Pango::FontDescription( font_name );
            this->layout->set_font_description( this->font_desc );
        }
    protected:
        bool on_draw( const Cairo::RefPtr<Cairo::Context> & cairo_context ) override
        {
            cairo_context->save();

            //draw background color
            if ( pointer_enters )
                set_rgba( cairo_context , SELECT_NUMBER_RGBA );
            else
                set_rgba( cairo_context , BOARD_BACKGROUD_RGBA );
            cairo_context->rectangle( 0 , 0 , this->get_allocated_width() , this->get_allocated_height() );
            cairo_context->stroke_preserve();
            cairo_context->fill();

            set_rgba( cairo_context , BUTTON_BORDER_RGBA );
            cairo_context->rectangle( 0 , 0 , this->get_allocated_width() , this->get_allocated_height() );
            cairo_context->stroke();
            cairo_context->fill();

            set_rgba( cairo_context , PUZZLE_NUMBER_RGBA );
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

int main( void )
{
    auto app = Gtk::Application::create();

    Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create_from_file( UI_FILE );

    Gtk::Window * window;
    builder->get_widget( "GameWindow" , window );

    Gtk::HeaderBar * headbar;
    builder->get_widget( "GameBar" , headbar );
    headbar->set_title( dump_level( NEW_GAME_LEVEL ) );

    SudokuBoard * sudoku_board;
    builder->get_widget_derived( "SudokuBoard" , sudoku_board );

    //bind setting menu buttons signal handler

    Gtk::CheckMenuItem * fill_mode;
    builder->get_widget( "FillMode" , fill_mode );
    fill_mode->signal_toggled().connect(
        [ sudoku_board , fill_mode ]()
        {
            if ( fill_mode->get_active() )
            {
                sudoku_board->set_fill_mode( SudokuBoard::FillMode::ANSWER );
                fill_mode->set_label( "FillAnswer" );
            }
            else
            {
                sudoku_board->set_fill_mode( SudokuBoard::FillMode::CANDIDATE );
                fill_mode->set_label( "FillCandidate" );
            }
        }
    );

    Gtk::CheckMenuItem * auto_update;
    builder->get_widget( "AutoUpdate" , auto_update );
    auto_update->signal_toggled().connect(
        [ sudoku_board , auto_update ]()
        {
            sudoku_board->auto_update_candidate( auto_update->get_active() );
        }
    );

    Gtk::MenuItem * light_theme;
    builder->get_widget( "LightTheme" , light_theme );
    light_theme->signal_activate().connect( 
        [ sudoku_board ]()
        {
            set_theme( Themes::LIGHT );
            sudoku_board->queue_draw();
        }
    );

    Gtk::MenuItem * dark_theme;
    builder->get_widget( "DarkTheme" , dark_theme );
    dark_theme->signal_activate().connect(
        [ sudoku_board ]()
        {
            set_theme( Themes::DARK );
            sudoku_board->queue_draw();
        }
    );

    Gtk::CheckMenuItem * view_solution;
    builder->get_widget( "ViewSolution" , view_solution );
    view_solution->signal_toggled().connect(
        [ sudoku_board , view_solution ]()
        {
            auto state = sudoku_board->get_game_state();
            if ( ( state != SudokuBoard::GameState::PLAYING ) && ( state != SudokuBoard::GameState::VIEW_SOLUTION ) )
                return ;
            if ( view_solution->get_active() )
            {
                sudoku_board->set_game_state( SudokuBoard::GameState::VIEW_SOLUTION );
            }
            else
            {
                sudoku_board->set_game_state( SudokuBoard::GameState::PLAYING );
            }
        }
    );

    Gtk::MenuItem * import_game;
    builder->get_widget( "ImportGame" , import_game );
    import_game->signal_activate().connect(
        [ sudoku_board ]()
        {
            auto clipboard = Gtk::Clipboard::get();
            clipboard->request_text( sigc::mem_fun<Glib::ustring>( sudoku_board , &SudokuBoard::new_game ) );
        }
    );

    Gtk::MenuItem * export_game;
    builder->get_widget( "ExportGame" , export_game );
    export_game->signal_activate().connect(
        [ sudoku_board ]()
        {
            auto clipboard = Gtk::Clipboard::get();
            clipboard->set_text( sudoku_board->dump_sudoku() );
        }
    );

    Gtk::MenuItem * print_sudoku;
    builder->get_widget( "PrintSudoku" , print_sudoku );
    print_sudoku->signal_activate().connect(
        [ sudoku_board ]()
        {
            Gtk::FileChooserDialog chooser( "select file" , Gtk::FileChooserAction::FILE_CHOOSER_ACTION_SAVE );
            auto filter = Gtk::FileFilter::create();
            filter->add_pattern( "*.[Pp][Nn][Gg]" );
            filter->add_pattern( "*.[Pp][Ss]" );
            filter->add_pattern( "*.[Ss][Vv][Gg]" );
            filter->add_pattern( "*.[Pp][Dd][Ff]" );
            filter->set_name( "png/svg/ps/pdf" );
            chooser.add_filter( filter );
            chooser.add_button( "OK" , 0 );
            chooser.run();
            std::filesystem::path filename( chooser.get_filename() );
            SudokuBoard::PrintType type = SudokuBoard::PrintType::PNG;
            Glib::ustring extension_string = filename.extension().string();
            extension_string = extension_string.lowercase();
            if ( extension_string == ".png" )
            {
                type = SudokuBoard::PrintType::PNG;
            }
            else if ( extension_string == ".ps" )
            {
                type = SudokuBoard::PrintType::PS;
            }
            else if ( extension_string == ".svg" )
            {
                type = SudokuBoard::PrintType::SVG;
            }
            else if ( extension_string == ".pdf" )
            {
                type = SudokuBoard::PrintType::PDF;
            }

            sudoku_board->print_puzzle( filename.string() , type );
        }
    );

    Gtk::MenuItem * exit_game;
    builder->get_widget( "ExitGame" , exit_game );
    exit_game->signal_activate().connect(
        [ app ]()
        {
            app->quit();
        }
    );

    //bind popover menus signal handler

    Gtk::Popover * level_menu;
    builder->get_widget( "LevelMenu" , level_menu );
    Gtk::Grid * level_grid;
    builder->get_widget( "LevelGrid" , level_grid );
    std::array< ControlButton , static_cast<std::size_t>( SUDOKU_LEVEL::_LEVEL_COUNT ) + 1 > level_button_arr;
    for( cell_t i = 0 ; i < static_cast<cell_t>( SUDOKU_LEVEL::_LEVEL_COUNT ) ; i++ )
    {
        level_button_arr[i].set_label( dump_level( static_cast<SUDOKU_LEVEL>( i ) ) );
        level_button_arr[i].set_font( "Ubuntu Mono 14" );
        level_button_arr[i].signal_button_press_event().connect(
            [ sudoku_board , level_menu , headbar , i ]( GdkEventButton * event )
            {
                //ignore double-clicked and three-clicked 
                if ( ( event->type == GdkEventType::GDK_2BUTTON_PRESS ) || ( event->type == GdkEventType::GDK_3BUTTON_PRESS ) )
                    return true;

                level_menu->popdown();
                headbar->set_title( dump_level( static_cast<SUDOKU_LEVEL>( i ) ) );
                sudoku_board->new_game( static_cast<SUDOKU_LEVEL>( i ) );
                return true;
            }
        );
        level_grid->attach( level_button_arr[i] , 0 , i , 1 , 1 );
    }

    int generator_pos = static_cast<int>( SUDOKU_LEVEL::_LEVEL_COUNT );
    level_button_arr[ generator_pos ].set_font( "Ubuntu Mono 14" );
    level_button_arr[ generator_pos ].set_label( "Automatic Generate" );
    level_button_arr[ generator_pos ].signal_button_press_event().connect(
        [ sudoku_board , level_menu , headbar ]( GdkEventButton * event )
        {
            //ignore double-clicked and three-clicked 
            if ( ( event->type == GdkEventType::GDK_2BUTTON_PRESS ) || ( event->type == GdkEventType::GDK_3BUTTON_PRESS ) )
                return true;

            level_menu->popdown();
            headbar->set_title( "Automatic Generate" );
            sudoku_board->new_game();
            return true;
        }
    );
    level_grid->attach( level_button_arr[ generator_pos ] , 0 , generator_pos , 1 , 1 );
    level_grid->show_all();

    Gtk::Popover * fill_menu;
    builder->get_widget( "FillMenu" , fill_menu );
    Gtk::Grid * fill_grid;
    builder->get_widget( "FillGrid" , fill_grid );
    fill_grid->show_all();
    sudoku_board->signal_button_press_event().connect(
        [ sudoku_board , fill_menu ]( GdkEventButton * event )
        {
            //ignore double-clicked and three-clicked 
            if ( ( event->type == GdkEventType::GDK_2BUTTON_PRESS ) || ( event->type == GdkEventType::GDK_3BUTTON_PRESS ) )
                return false;
            
            if ( fill_menu->is_visible() )
            {
                fill_menu->popdown();
                return false;
            }

            if ( sudoku_board->can_fill( event->x , event->y ) == false )
                return false;

            fill_menu->set_pointing_to( { static_cast<gint>( event->x ) , static_cast<gint>( event->y ) , 0 , 0 } );
            fill_menu->popup();

            //call default signal handler
            return false;
        },
        true
    );
    for( cell_t i = 0 ; i < SUDOKU_SIZE ; i++ )
    {
        ControlButton * fill_button;
        builder->get_widget_derived( "Fill" + std::to_string( i + 1 ) , fill_button , std::to_string( i + 1 ) , "Ubuntu Mono 14" );
        fill_button->signal_button_press_event().connect(
            [ sudoku_board , fill_menu , i ]( GdkEventButton * event )
            {
                //ignore double-clicked and three-clicked 
                if ( ( event->type == GdkEventType::GDK_2BUTTON_PRESS ) || ( event->type == GdkEventType::GDK_3BUTTON_PRESS ) )
                    return true;

                sudoku_board->do_fill( i + 1 );
                fill_menu->popdown();
                return true;
            }
        );
    }

    //bind toolbar buttons signal handler

    Gtk::Button * new_game_button;
    builder->get_widget( "NewGame" , new_game_button );
    new_game_button->signal_clicked().connect(
        [ level_menu ]()
        {
            Gtk::Widget * relative_widget = level_menu->get_relative_to();
            level_menu->set_size_request( relative_widget->get_allocated_width() );
            level_menu->set_pointing_to( { relative_widget->get_allocated_width()/2 , relative_widget->get_allocated_height()/2 , 0 , 0 } );
            level_menu->popup();
        }
    );

    Gtk::Button * undo_button;
    builder->get_widget( "Undo" , undo_button );
    undo_button->signal_clicked().connect(
        [ sudoku_board ]()
        {
            sudoku_board->undo_fill();
        }
    );

    Gtk::Button * redo_button;
    builder->get_widget( "Redo" , redo_button );
    redo_button->signal_clicked().connect(
        [ sudoku_board ]()
        {
            sudoku_board->redo_fill();
        }
    );

    Gtk::Widget * playing_image;
    builder->get_widget( "PlayingImage" , playing_image );
    Gtk::Widget * pause_image;
    builder->get_widget( "PauseImage" , pause_image );
    Gtk::Button * play_status_button;
    builder->get_widget( "ChangePlayState" , play_status_button );
    play_status_button->signal_clicked().connect(
        [ sudoku_board , play_status_button , playing_image , pause_image ]()
        {
            SudokuBoard::GameState state = sudoku_board->get_game_state();
            if ( state == SudokuBoard::GameState::PLAYING )
            {
                sudoku_board->set_game_state( SudokuBoard::GameState::PAUSE );
                play_status_button->set_image( *playing_image );
            }
            else if ( state == SudokuBoard::GameState::PAUSE )
            {
                sudoku_board->set_game_state( SudokuBoard::GameState::PLAYING );
                play_status_button->set_image( *pause_image );
            }
        }
    );

    Gtk::Label * timer_label;
    builder->get_widget( "TimerLabel" , timer_label );
    Glib::signal_timeout().connect_seconds(
        [ sudoku_board , timer_label ]()
        {
            timer_label->set_label( sudoku_board->dump_play_time() );
            return true;
        },
        1 
    );

    window->show_all();
    return app->run( *window );
}
