/* vim: :se ai :se sw=4 :se ts=4 :se sts :se et */


/*H**********************************************************************
 *
 *    This is a skeleton to guide development of Othello engines that can be used
 *    with the Ingenious Framework and a Tournament Engine. 
 *
 *    The communication with the referee is handled by an implementaiton of comms.h,
 *    All communication is performed at rank 0.
 *
 *    Board co-ordinates for moves start at the top left corner of the board i.e.
 *    if your engine wishes to place a piece at the top left corner, 
 *    the "gen_move_master" function must return "00".
 *
 *    The match is played by making alternating calls to each engine's 
 *    "gen_move_master" and "apply_opp_move" functions. 
 *    The progression of a match is as follows:
 *        1. Call gen_move_master for black player
 *        2. Call apply_opp_move for white player, providing the black player's move
 *        3. Call gen move for white player
 *        4. Call apply_opp_move for black player, providing the white player's move
 *        .
 *        .
 *        .
 *        N. A player makes the final move and "game_over" is called for both players
 *    
 *    IMPORTANT NOTE:
 *        Write any (debugging) output you would like to see to a file. 
 *        	- This can be done using file fp, and fprintf()
 *        	- Don't forget to flush the stream
 *        	- Write a method to make this easier
 *        In a multiprocessor version 
 *        	- each process should write debug info to its own file 
 *H***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <mpi.h>
#include <time.h>
#include <assert.h>
#include "comms.h"

const int EMPTY = 0;
const int BLACK = 1;
const int WHITE = 2;

const int OUTER = 3;
const int ALLDIRECTIONS[8] = {-11, -10, -9, -1, 1, 9, 10, 11};
const int BOARDSIZE = 100;

const int LEGALMOVSBUFSIZE = 65;
const char piecenames[4] = {'.','b','w','?'};

void run_master(int argc, char *argv[]);
int initialise_master(int argc, char *argv[], int *time_limit, int *my_colour, FILE **fp);
void gen_move_master(char *move, int my_colour, FILE *fp);
void apply_opp_move(char *move, int my_colour, FILE *fp);
void game_over();
void run_worker();
void initialise_board();
void free_board();
void legal_moves(int player, int *moves, FILE *fp);
int legalp(int move, int player, FILE *fp);
int validp(int move);
int would_flip(int move, int dir, int player, FILE *fp);
int opponent(int player, FILE *fp);
int find_bracket_piece(int square, int dir, int player, FILE *fp);
int random_strategy(int my_colour, FILE *fp);
void random_strategy_2(int *moves, int buffer_size, int *best_move);
void make_move(int move, int player, FILE *fp);
void make_flips(int move, int dir, int player, FILE *fp);
int get_loc(char* movestring);
void get_move_string(int loc, char *ms);
void print_board(FILE *fp);
char nameof(int piece);
int count(int player, int * board);
FILE* open_logfile(int colour);
void close_logfile(FILE* fptr);
FILE* open_logfile1(int colour);
FILE* open_logfile_2(int colour);
void gen_move_master3(char *move, int my_colour, FILE *fp, FILE*masterPtr);
void search_for_best_move(int *moves, int buffer_size, int *best_move, int player, FILE *ptr);
int minimax(int *board, int move, int depth, int maximizing_player, int current_player, int alpha, int beta, FILE *ptr);

//The functions below are copies of the function provided in the skeleton code with a few minor changes
//Instead of changing the contents of the global board they all work together to change the state of a local board sent in the parameters
void legal_moves_1(int player, int *moves, int *board_modified); 
int legalp_1(int move, int player, int* board_modified);
void make_modified_move(int move, int *board_modified, int player);
void make_flips_1(int move, int dir, int player, int *board_modified);
int would_flip_1(int move, int dir, int player, int *board_modified);
int opponent_1(int player);
int find_bracket_piece_1(int square, int dir, int player, int *board_modified);
int static_evaluation(int *board_modified, int player_type, FILE *ptr);
void print_board_1(FILE *fp, int *local_board);

int *board;

int main(int argc, char *argv[]) {
	int rank;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
 
	initialise_board(); //one for each process

	if (rank == 0) {
	    run_master(argc, argv);
	} else {
	    run_worker(rank);
	}
	game_over();
}

void run_master(int argc, char *argv[]) {
	char cmd[CMDBUFSIZE];
	char my_move[MOVEBUFSIZE];
	char opponent_move[MOVEBUFSIZE];
	int time_limit;
	int my_colour;
	int running = 0;
	FILE *fp = NULL;

	if (initialise_master(argc, argv, &time_limit, &my_colour, &fp) != FAILURE) {
		running = 1;
	}
	if (my_colour == EMPTY) my_colour = BLACK;
	// Broadcast my_colour
	//Sends the colour of the player to all of the processes
	MPI_Bcast(&my_colour, 1, MPI_INT, 0, MPI_COMM_WORLD);
	
	//File pointer created to print to a file everything that happens in the master process(process 0)
	FILE *masterPtr = open_logfile1(my_colour);
	fprintf(masterPtr, "Sam you beauty, your colour is %d\n", my_colour);

	while (running == 1) {
		/* Receive next command from referee */
		if (comms_get_cmd(cmd, opponent_move) == FAILURE) {
			fprintf(fp, "Error getting cmd\n");
			fflush(fp);
			running = 0;
			break;
		}

		/* Received game_over message */
		if (strcmp(cmd, "game_over") == 0) {
			running = 0;
			fprintf(fp, "Game over\n");
			close_logfile(masterPtr);
			fflush(fp);
			break;

		/* Received gen_move message */
		} else if (strcmp(cmd, "gen_move") == 0) {
			// Broadcast running
			//When the player receives a generate move command it sends it to all the processes and they begin to execute
			MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);
			// Broadcast board 
			//The board is broadcasted to all the processes
			MPI_Bcast(board, 100, MPI_INT, 0, MPI_COMM_WORLD);
			//The function below retrieves the best move, puts it into string format and then places it in the my_move variable
			//The function coordinates the evaluation of all of the legal moves
			gen_move_master3(my_move, my_colour, fp, masterPtr);
			
			//gen_move_master(my_move, my_colour, fp);
			print_board(fp);

			if (comms_send_move(my_move) == FAILURE) { 
				running = 0;
				fprintf(fp, "Move send failed\n");
				fflush(fp);
				break;
			}

		/* Received opponent's move (play_move mesage) */
		} else if (strcmp(cmd, "play_move") == 0) {
			apply_opp_move(opponent_move, my_colour, fp);
			print_board(fp);

		/* Received unknown message */
		} else {
			fprintf(fp, "Received unknown command from referee\n");
		}
		
	}
	// Broadcast running
	MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);
}

int initialise_master(int argc, char *argv[], int *time_limit, int *my_colour, FILE **fp) {
	int result = FAILURE;

	if (argc == 5) { 
		unsigned long ip = inet_addr(argv[1]);
		int port = atoi(argv[2]);
		*time_limit = atoi(argv[3]);

		*fp = fopen(argv[4], "w");
		if (*fp != NULL) {
			fprintf(*fp, "Initialise communication and get player colour \n");
			if (comms_init_network(my_colour, ip, port) != FAILURE) {
				result = SUCCESS;
			}
			fflush(*fp);
		} else {
			fprintf(stderr, "File %s could not be opened", argv[4]);
		}
	} else {
		fprintf(*fp, "Arguments: <ip> <port> <time_limit> <filename> \n");
	}
	
	return result;
}

void initialise_board() {
	int i;
	board = (int *) malloc(BOARDSIZE * sizeof(int));
	for (i = 0; i <= 9; i++) board[i] = OUTER;
	for (i = 10; i <= 89; i++) {
		if (i%10 >= 1 && i%10 <= 8) board[i] = EMPTY; else board[i] = OUTER;
	}
	for (i = 90; i <= 99; i++) board[i] = OUTER;
	board[44] = WHITE; board[45] = BLACK; board[54] = BLACK; board[55] = WHITE;
}

void free_board() {
	free(board);
}

/**
 *   Rank i (i != 0) executes this code 
 *   ----------------------------------
 *   Called at the start of execution on all ranks except for rank 0.
 *   - run_worker should play minimax from its move(s) 
 *   - results should be send to Rank 0 for final selection of a move 
 */

void run_worker() {

	//The number of processes is loarded in the comm_sz integer variable
	int comm_sz;
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
	//The rank of each process is loaded into the my_rank variable
	int my_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	int running = 0;
	int my_colour;

	// Broadcast colour
	MPI_Bcast(&my_colour, 1, MPI_INT, 0, MPI_COMM_WORLD);
	FILE *slavePtr = open_logfile(my_colour);

	// Broadcast running	
    MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);


	while (running == 1) {
		// Broadcast board
		MPI_Bcast(board, 100, MPI_INT, 0, MPI_COMM_WORLD);
		

		//moves variables
		int buffer_size = 0;
		int *receive_buffer = (int*)malloc(sizeof(int) * 15); 
		int *send_counts = (int*)malloc(sizeof(int) * comm_sz);
		//The send_counts array is broadcasted to all the processes. It specifies the buffer size of each of the processes.
		//The processes will not always have the same number of elements but the biggest difference in buffer size will always be 1.
		MPI_Bcast(send_counts, comm_sz, MPI_INT, 0, MPI_COMM_WORLD);

		// Generate move        
		for (int i = 1; i < comm_sz; i++)
		{
			if (my_rank == i)
			{	
				//buffer_size variable for each process
				buffer_size = send_counts[i];
			}
		} 
		//Receives a subset of all the legal moves and loads it into the variable receive_buffer
		MPI_Scatterv(NULL, NULL, NULL, MPI_DATATYPE_NULL, receive_buffer, buffer_size, MPI_INT, 0, MPI_COMM_WORLD);
		//fprintf(slavePtr, "The receive buffer of process %d looks like ", my_rank);
		for (int j = 0; j < buffer_size; j++)
		{
			fprintf(slavePtr, "%d\t", receive_buffer[j]);
		}
		fprintf(slavePtr, "\n");
		free(send_counts);
		int *best_move = (int*)malloc(sizeof(int) * 2);
		//random_strategy_2(receive_buffer, buffer_size, best_move);
		//Function loads the best move and its evaluation in the array best move
		//The best move is placed at index 0 of the array
		//The evaluation of that move is placed at index 1 of the array
		search_for_best_move(receive_buffer, buffer_size, best_move, my_colour, slavePtr);
		//The gather function joins all of the best_move arrays into one array and sends this array to process 0
		MPI_Gatherv(best_move, 2, MPI_INT, NULL, NULL, NULL, MPI_DATATYPE_NULL, 0, MPI_COMM_WORLD);
		//fprintf(slavePtr, "The best move of the subset of moves is %d with an evaluation of %d\n", best_move[0], best_move[1]);
		//fprintf(slavePtr, "\n");
		free(best_move);

		// Broadcast running
		MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);
	}
}

/**
 *  Rank 0 executes this code: 
 *  --------------------------
 *  Called when the next move should be generated 
 *  - gen_move_master should play minimax from its move(s)
 *  - the ranks may communicate during execution 
 *  - final results should be gathered at rank 0 for final selection of a move 
 */

void gen_move_master(char *move, int my_colour, FILE *fp) {
	int loc;

	/* generate move */
	loc = random_strategy(my_colour, fp);

	if (loc == -1) {
		strncpy(move, "pass\n", MOVEBUFSIZE);
	} else {
		/* apply move */
		get_move_string(loc, move);
		make_move(loc, my_colour, fp);
	}
}

void gen_move_master3(char *move, int my_colour, FILE *fp, FILE*masterPtr) {
	
	int comm_sz;
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
	int *all_legal_moves = (int*)malloc(sizeof(int) * LEGALMOVSBUFSIZE);
	memset(all_legal_moves, 0, LEGALMOVSBUFSIZE);
	legal_moves(my_colour, all_legal_moves, fp);

	int number_legal_moves = all_legal_moves[0];
	int elements_per_process = number_legal_moves/comm_sz;
	int remainder = number_legal_moves%comm_sz;
	int *receive_buffer = (int*)malloc(sizeof(int) * (elements_per_process+1));
	int *send_counts = (int*)malloc(sizeof(int) * comm_sz);
	int *displs = (int*)malloc(sizeof(int) * comm_sz);
	int sum = 0;

	//The for loop below fills the array send_counts up with the number of legal moves each process will receive
	//For example is there is 14 legal moves and there are 4 processes then: Process0 = 4; Process1 = 4; Process2 = 3; Process3 = 3
	//It divides the legal moves between the processes in a sensible way
	for (int i = 0; i < comm_sz; i++)
	{
		send_counts[i] = elements_per_process;
		if (remainder > 0)
		{
			send_counts[i]++;
			remainder--;
		}
		displs[i] = sum;
		sum = sum + send_counts[i];
		//fprintf(masterPtr, "The number of elements that are sent to process %d is %d\n", i, send_counts[i]);
		//fprintf(masterPtr, "The displacement is %d\n", displs[i]);
	}

	fprintf(masterPtr, "\n");
	//The for loop below merely shifts the array so that the first legal move is at index 0 of the array
	for (int j = 0; j < number_legal_moves; j++)
	{
		all_legal_moves[j] = all_legal_moves[j+1];
	}

	int buffer_size = send_counts[0];
	//MPI_Bcast function sends the send_counts array to each of the processes
	MPI_Bcast(send_counts, comm_sz, MPI_INT, 0, MPI_COMM_WORLD);
	//The scatter function breaks the all_legal_moves array in smaller arrays and sends these array to their respective processes
	MPI_Scatterv(all_legal_moves, send_counts, displs, MPI_INT, receive_buffer, buffer_size, MPI_INT, 0, MPI_COMM_WORLD);
	free(all_legal_moves);
	free(send_counts);
	free(displs); 
	//fprintf(masterPtr, "The receive buffer of process 0 looks like ");
	/*
	for (int p = 0; p < buffer_size; p++)
	{
		fprintf(masterPtr, "%d\t", receive_buffer[p]);
	}
	fprintf(masterPtr, "\n"); */

	int *best_move = (int*)malloc(sizeof(int) * 2);
	//random_strategy_2(receive_buffer, buffer_size, best_move);
	search_for_best_move(receive_buffer, buffer_size, best_move, my_colour, masterPtr);
	int *receive_counts = (int*)malloc(sizeof(int) * comm_sz);
	int sum_2 = 0;
	int *displs_rec = (int*)malloc(sizeof(int) * comm_sz);
	int *receive_buffer_best_moves = (int*)malloc(sizeof(int) * 2 * comm_sz);
	//fprintf(masterPtr, "The best move of the subset of moves is %d with an evaluation of %d\n", best_move[0], best_move[1]);
	//The for loop fills an array receive_count with two
	//It also fills the displacement array
	//This is preparation of MPI_Gatherv function
	for (int k = 0; k < comm_sz; k++)
	{
		receive_counts[k] = 2;
		displs_rec[k] = sum_2;
		sum_2 = sum_2 + receive_counts[k];
	}
	//The best moves and their evuluations are loaded into receive_buffer_best_move
	MPI_Gatherv(best_move, 2, MPI_INT, receive_buffer_best_moves, receive_counts, displs_rec, MPI_INT, 0, MPI_COMM_WORLD);
	free(displs_rec);
	free(receive_counts);
	free(best_move);
	int best_move_loc = -1;
	int evaluation = -100;
	//The for loop below determines the very best move out of all the best moves of the subset of moves. 
	for (int l = 1; l < (2*comm_sz); l=l+2)
	{
		fprintf(masterPtr, "A best move of the subset of moves is %d with an evaluation of %d\n", receive_buffer_best_moves[l-1], receive_buffer_best_moves[l]);
		if (receive_buffer_best_moves[l] > evaluation)
		{
			evaluation = receive_buffer_best_moves[l];
			best_move_loc = receive_buffer_best_moves[l-1];
		}
	}

	free(receive_buffer_best_moves);
	fprintf(masterPtr, "The very best move is %d with an evaluation of %d\n", best_move_loc, evaluation);
	

	int loc = best_move_loc;
	//int loc = random_strategy(my_colour, fp);

	if (loc == -1) {
		strncpy(move, "pass\n", MOVEBUFSIZE);
	} else {
		/* apply move */
		get_move_string(loc, move);
		make_move(loc, my_colour, fp);
	}
}

void apply_opp_move(char *move, int my_colour, FILE *fp) {
	int loc;
	if (strcmp(move, "pass\n") == 0) {
		return;
	}
	loc = get_loc(move);
	make_move(loc, opponent(my_colour, fp), fp);
}

void game_over() {
	free_board();
	MPI_Finalize();
}

void get_move_string(int loc, char *ms) {
	int row, col, new_loc;
	new_loc = loc - (9 + 2 * (loc / 10));
	row = new_loc / 8;
	col = new_loc % 8;
	ms[0] = row + '0';
	ms[1] = col + '0';
	ms[2] = '\n';
	ms[3] = 0;
}

int get_loc(char* movestring) {
	int row, col;
	/* movestring of form "xy", x = row and y = column */ 
	row = movestring[0] - '0'; 
	col = movestring[1] - '0'; 
	return (10 * (row + 1)) + col + 1;
}

void legal_moves(int player, int *moves, FILE *fp) {
	int move, i;
	moves[0] = 0;
	i = 0;
	for (move = 11; move <= 88; move++)
		if (legalp(move, player, fp)) {
		i++;
		moves[i] = move;
	}
	moves[0] = i;
}

int legalp(int move, int player, FILE *fp) {
	int i;
	if (!validp(move)) return 0;
	if (board[move] == EMPTY) {
		i = 0;
		while (i <= 7 && !would_flip(move, ALLDIRECTIONS[i], player, fp)) i++;
		if (i == 8) return 0; else return 1;
	}
	else return 0;
}

int validp(int move) {
	if ((move >= 11) && (move <= 88) && (move%10 >= 1) && (move%10 <= 8))
		return 1;
	else return 0;
}

int would_flip(int move, int dir, int player, FILE *fp) {
	int c;
	c = move + dir;
	if (board[c] == opponent(player, fp))
		return find_bracket_piece(c+dir, dir, player, fp);
	else return 0;
}

int find_bracket_piece(int square, int dir, int player, FILE *fp) {
	while (board[square] == opponent(player, fp)) square = square + dir;
	if (board[square] == player) return square;
	else return 0;
}

int opponent(int player, FILE *fp) {
	if (player == BLACK) return WHITE;
	if (player == WHITE) return BLACK;
	fprintf(fp, "illegal player\n"); return EMPTY;
}

int random_strategy(int my_colour, FILE *fp) {
	int r;
	int *moves = (int *) malloc(LEGALMOVSBUFSIZE * sizeof(int));
	memset(moves, 0, LEGALMOVSBUFSIZE);

	legal_moves(my_colour, moves, fp);
	if (moves[0] == 0) {
		return -1;
	}
	srand (time(NULL));
	r = moves[(rand() % moves[0]) + 1];
	free(moves);
	return(r);
}

void random_strategy_2(int *moves, int buffer_size, int* best_move) {

	if (buffer_size == 0) {
		best_move[0] = -1;
		best_move[1] = -2;
		return;
	}
	srand (time(NULL));
	best_move[0] = moves[rand() % buffer_size];
	best_move[1] = rand()%10;
	free(moves);
}

void make_move(int move, int player, FILE *fp) {
	int i;
	board[move] = player;
	for (i = 0; i <= 7; i++) make_flips(move, ALLDIRECTIONS[i], player, fp);
}

void make_flips(int move, int dir, int player, FILE *fp) {
	int bracketer, c;
	bracketer = would_flip(move, dir, player, fp);
	if (bracketer) {
		c = move + dir;
		do {
			board[c] = player;
			c = c + dir;
		} while (c != bracketer);
	}
}

void print_board(FILE *fp) {
	int row, col;
	fprintf(fp, "   1 2 3 4 5 6 7 8 [%c=%d %c=%d]\n",
		nameof(BLACK), count(BLACK, board), nameof(WHITE), count(WHITE, board));
	for (row = 1; row <= 8; row++) {
		fprintf(fp, "%d  ", row);
		for (col = 1; col <= 8; col++)
			fprintf(fp, "%c ", nameof(board[col + (10 * row)]));
		fprintf(fp, "\n");
	}
	fflush(fp);
}

char nameof(int piece) {
	assert(0 <= piece && piece < 5);
	return(piecenames[piece]);
}

int count(int player, int * board) {
	int i, cnt;
	cnt = 0;
	for (i = 1; i <= 88; i++)
		if (board[i] == player) cnt++;
	return cnt;
}

FILE* open_logfile(int colour)
{
	FILE *filePtr = NULL;
	char *filename = NULL;
	int rank;

	if (!filename)
	{
		filename = malloc(64*sizeof(char));
		if (filename)
		{
			MPI_Comm_rank(MPI_COMM_WORLD, &rank);
			sprintf(filename, "player_%d_process_%d.log", colour, rank);
		}
		else
		{
			return NULL;
		}
	}

	filePtr = fopen(filename, "a");

	if (!filePtr)
	{
		return NULL;
	}

	free(filename);

	return filePtr;
}

void close_logfile(FILE* fptr)
{
	fflush(fptr);
	fclose(fptr);
}

FILE* open_logfile1(int colour)
{
	FILE *filePtr = NULL;
	char *filename = NULL;

	if (!filename)
	{
		filename = malloc(64*sizeof(char));
		if (filename)
		{
			sprintf(filename, "Master_player_%d.log", colour);
		}
		else
		{
			return NULL;
		}
	}

	filePtr = fopen(filename, "a");

	if (!filePtr)
	{
		return NULL;
	}

	free(filename);

	return filePtr;
}

FILE* open_logfile_2(int colour)
{
	FILE *filePtr = NULL;
	char *filename = NULL;

	if (!filename)
	{
		filename = malloc(64*sizeof(char));
		if (filename)
		{
			sprintf(filename, "Error_player_%d.log", colour);
		}
		else
		{
			return NULL;
		}
	}

	filePtr = fopen(filename, "a");

	if (!filePtr)
	{
		return NULL;
	}

	free(filename);

	return filePtr;
}

void search_for_best_move(int *moves, int buffer_size, int *best_move, int player, FILE *ptr)
{
	//This places the best move and its evaluation into the best_move array

	if (buffer_size == 0)
	{
		best_move[0] = -1;
		best_move[1] = -100;
		return;
	}

	//A local copy of the board is made
	int *local_board = (int*)malloc(sizeof(int) * 100);		
	for (int a = 0; a < 100; a++)
	{
	local_board[a] = board[a];
	}


	best_move[0] = -1;
	best_move[1] = -100;
	int max = -100;
	int num = 0;
	int evaluation;

	for (int i = 0; i < buffer_size; i++)
	{	
		//Every legal move in the buffer of the process is evaluated and the one with the highest evaluation is placed in the best_move array
		evaluation = minimax(local_board, moves[i], 6, player, player, -1000, 1000, ptr);
		//fprintf(ptr, "One of the moves in the buffer is %d and it has an evaluation of %d\n", moves[i], evaluation);
		if (evaluation > max)
		{
			max = evaluation;
			num = i;
		}	
	}
	fprintf(ptr, "\n");
	best_move[0] = moves[num];
	best_move[1] = max; 
	free(local_board);
}

int minimax(int *local_board, int move, int depth, int maximizing_player, int current_player, int alpha, int beta, FILE *ptr)
{

	int score = 0;

	if (move == 0)
	{
		return 0;
	}

	//Uses the move the change the state of the board and loads the new state into local_board
	make_modified_move(move, local_board, current_player);//Changes the local_board appropriately
	current_player = opponent_1(current_player);//Changes the current player
	int *moves = (int*)malloc(sizeof(int) * LEGALMOVSBUFSIZE);
	legal_moves_1(current_player, moves, local_board);//Determines the legal moves for the new board and current player
	int total_moves = moves[0];

	if (depth == 0 || total_moves == 0)
	{
	//Performs static evuluation of the board
	//It calculates the static evaluation by subtracting the number of squares of the minimizing players from the number of squares of the maximizing player
	score = static_evaluation(local_board, maximizing_player, ptr);
	return score;
	}

	//A copy of the local board is made
	int *temp_board = (int*)malloc(sizeof(int) * 100);
	for (int i = 0; i < 100; i++)
	{
		temp_board[i] = local_board[i];
	}

	if (current_player == maximizing_player)
	{
		int maxEval = -100;
		int eval;
		for (int j = 1; j <= total_moves; j++)
		{
			eval = minimax(temp_board, moves[j], depth-1, maximizing_player, current_player, alpha, beta, ptr);
			if (eval > maxEval)
			{
				maxEval = eval;
			}
			//Alpha beta pruning
			if (alpha < score)
			{
				alpha = score;
			}
			if (beta <= alpha)
			{
				break;
			}
			for (int t = 0; t < 100; t++)
			{
				temp_board[t] = local_board[t];
			}
		}
		free(temp_board);
		free(moves);
		return maxEval;
	}
	else
	{
		int minEval = 100;
		int eval;
		for (int k = 1; k <= total_moves; k++)
		{
			eval = minimax(temp_board, moves[k], depth-1, maximizing_player, current_player, alpha, beta, ptr);
			if (eval < minEval)
			{
				minEval = eval;
			}
			if (score < beta)
			{
				beta = score;
			}
			if (beta <= alpha)
			{
				break;
			}
			for (int b = 0; b < 100; b++)
			{
				temp_board[b] = local_board[b];
			}
		}
		free(temp_board);
		free(moves);
		return minEval;
	}

	
}

void make_modified_move(int move, int *board_modified, int player)
{
	board_modified[move] = player;
	for (int i = 0; i <= 7; i++)
	{
		make_flips_1(move, ALLDIRECTIONS[i], player, board_modified);
	}
}

void make_flips_1(int move, int dir, int player, int *board_modified) {
	int bracketer, c;

	bracketer = would_flip_1(move, dir, player, board_modified);
	if (bracketer) {
		c = move + dir;
		do {
			board_modified[c] = player;
			c = c + dir;
		} while (c != bracketer);
	}
}

int would_flip_1(int move, int dir, int player, int *board_modified) {
	int c;
	c = move + dir;
	if (board_modified[c] == opponent_1(player))
		return find_bracket_piece_1(c+dir, dir, player, board_modified);
	else return 0;
}

int opponent_1(int player) {
	if (player == BLACK) return WHITE;
	if (player == WHITE) return BLACK;
	return EMPTY;
}

int find_bracket_piece_1(int square, int dir, int player, int *board_modified) {
	while (board_modified[square] == opponent_1(player)) square = square + dir;
	if (board_modified[square] == player) return square;
	else return 0;
}

void legal_moves_1(int player, int *moves, int *board_modified) {
	int move, i;
	moves[0] = 0;
	i = 0;
	for (move = 11; move <= 88; move++)
		if (legalp_1(move, player, board_modified)) {
		i++;
		moves[i] = move;
	}
	moves[0] = i;
}

int legalp_1(int move, int player, int* board_modified) {
	int i;
	if (!validp(move)) return 0;
	if (board_modified[move] == EMPTY) {
		i = 0;
		while (i <= 7 && !would_flip_1(move, ALLDIRECTIONS[i], player, board_modified)) i++;
		if (i == 8) return 0; else return 1;
	}
	else return 0;
}

int static_evaluation(int *board_modified, int player_type, FILE *ptr)
{
	int white = 0;
	int black = 0;
	int blank = 0;
	int statEval1 = 0;
	int statEval2 = 0;

	for (int i = 11; i <=88; i++)
	{
		if ((i%10 !=0 ) && (i%10 != 9))
		{
			if (board_modified[i] == WHITE)
			{
				white++;
			}
			else if (board_modified[i] == BLACK)
			{
				black++;
			}
			else if (board_modified[i] == EMPTY)
			{
				blank++;
			}
		}
	}

	if (player_type == WHITE)
	{
		statEval1 = white - black;
		//fprintf(ptr, "The player type is white and the static evaluation is %d\n", statEval1);
		return statEval1;
	}
	else if (player_type == BLACK)
	{
		statEval2 = black - white;
		//fprintf(ptr, "The player type is black and the static evaluation is %d\n", statEval2);
		return statEval2;
	}
	else
	{
		return blank;
	}
}

void print_board_1(FILE *fp, int *local_board) {
	int row, col;
	fprintf(fp, "   1 2 3 4 5 6 7 8 [%c=%d %c=%d]\n",
		nameof(BLACK), count(BLACK, local_board), nameof(WHITE), count(WHITE, local_board));
	for (row = 1; row <= 8; row++) {
		fprintf(fp, "%d  ", row);
		for (col = 1; col <= 8; col++)
			fprintf(fp, "%c ", nameof(local_board[col + (10 * row)]));
		fprintf(fp, "\n");
	}
	fflush(fp);
}
