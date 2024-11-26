/*** inludes start ***/

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>

/*** inludes end ***/

/*** defines start ***/

#define CTRL_KEY(k) ((k) & 0x1f)
//faccio l'AND tra k e 0x1f(0001 1111) e lo uso perchè così ogni numero a 8 bit (ASCII quindi) ha i primi 3 bit
//azzerati, questo mi permetto di usare i codici di controllo(che vanno da 0 a 31) senza fatica richiamando la macro
//e dandole in pasto un char

/***defines end***/

/***data start***/

struct editorConfig 
{
    int screenrows;
    int screencols;

    struct termios orig_termios;
};

struct editorConfig E;
//Usato per ripristinare il terminale dell'utente a sessione terminata.

/*** data end ***/

/*** terminal start ***/

void error(const char *s)//handlign del messaggio di errore
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
  	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);//funziona perchè const char *s è il primo
			 //carattere della stringa che "attiva" perror
	exit(1);
	
}

void returnCookedMode()
{
	  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) 
	  	error("tcgetattr");
	
}

void rawMode()
{
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) 
		error("tcgetattr");

	struct termios raw = E.orig_termios;

	//DISABILITA FLAG PER PERMETTERE EDITING NEL TEXT EDITOR E NON NEL TERMINALE
						
	//faccio un "and" e poi un "not" sui flag delle impostazioni del terminale per settare solo
	//e soltanto i flag che voglio io a off
	
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);//si occupa del terminale

	raw.c_iflag &= ~(ICRNL | IXON);//si occupa dell'input
	raw.c_oflag &= ~(OPOST);//si occupa dell'output

	//*** LISTA FLAG ***//

		//ICANON serve per leggere byte per byte e non stringhe intere, disattivo, quindi, la canonical mode.
		//ISIG disattiva ctrl-c/z/ che equivalgono al terminale il processo corrente, il primo, e sospenderlo il secondo
		//IEXTEN disattiva ctrl-v non pasando in input i caratteri scritti dopo averlo premuto
		//IXON disattiva ctrl-s/q che usiamo per sospendere e riprendere processi
		//ICRNL disattiva ctrl-m che riporta a capo dando in input quello che c'è scritto(carraige return on newline)
		//OPOST si occupa di disattivare la funzione automtica del terminale \r che riporta all'inizio della riga il cursore

	//*** FINE LISTA FLAG ***//

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) 
		error("tcsetattr");
	//flusho cos'ì tutto quello inserito prima della raw mode non verrà letto
											
	atexit(returnCookedMode);//viene chiamata alla fine dell'esecuzione del programma
}

char readKey(void)
{
	int check;
	char c;

	while((check = read(STDIN_FILENO, &c, 1)) != 1)//finchè non gli passo più di un char
	//avendo disattivato ICANON sarebbe anomalo avere più byte passati alla func
	{
		if(check == -1 && errno != EAGAIN)
			error("read");
	}

	return c;
}

int cursorPos(int *rows, int *cols)
{
	char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)//scriviamo al terminale cosa vogliamo in out
    //n lo usiamo per status info sul terminale la 6° info è quella riguardante la pos
    //del cursore
    	return -1;

    while (i < sizeof(buf) - 1) 
    {
    	if (read(STDIN_FILENO, &buf[i], 1) != 1)
			break;

        if (buf[i] == 'R') 
        	break;

        i++;
    }

    buf[i] = '\0';

    if (buf[1] == '[')
    	 return -1;

    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) 
    	return -1;

    return 0;

}

int getWindowSize(int *rows, int *cols) 
{
    struct winsize ws;
    
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) 
	{						//get win size
    	if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 10) != 10)
    	//B = cursod down C = Cursor Forward
    		return -1;

    	return cursorPos(rows, cols);
    } 
    else 
    {
      *cols = ws.ws_col;
      *rows = ws.ws_row;
      return 0;
  }
}





/*** terminal end ***/

/*** UI start ***/

void drawTilde() 
{
    int y;
    for (y = 0; y < E.screenrows; y++) 
    {
      write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void renderUI()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	//passiamo 4 byte al terminale:
	//1 byte: \x1b, che equivale all'escale character 27
	//2 byte: [, insieme a \x1b formano una escape sequence, ovvero diciamo al terminale come formattare il testo
	//3+4 byte: 2J, pulisce lo schermo(J) per intero(2)

	drawTilde();
	write(STDOUT_FILENO, "\x1b[H", 3);
	//H riposizione il cursore in 1;1 nel temrinale

}

/***UI end***/

/*** input start ***/

void keypress()
{
	char cPressed = readKey();	

	switch(cPressed)
	{
		case CTRL_KEY('q'):

  			write(STDOUT_FILENO, "\x1b[2J", 4);
  			write(STDOUT_FILENO, "\x1b[H", 3);

			exit(0);//usciamo quando il valore letto è pari al valore di q(01110001) 
									//"ANDato" con 0x1f(0001 1111) che quindi diventa ctrl+q
		break;
	}
}

/*** input end ***/

/*** initialization ***/

void initEditor() 
{
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
   	   error("getWindowSize");
}


int main()
{
	rawMode();
	initEditor();
	
	while (1) 
	{
		renderUI();
    	keypress();
  	}
	//leggiamo da readC 1 byte alla volta
	//dalla stream di input che si crea 
	//automaticamente quando avviamo il programma
	//quando premo invio tutti i caratteri vengono letti 
	//e se c'è una 'q' il programma esce e tutto quello
	//dopo la 'q' viene mandato al terminale.
														
	return 0;
}

