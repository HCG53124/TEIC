/*** inludes start ***/

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h> 

/*** inludes end ***/

/*** defines start ***/

#define TEIC_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)
//faccio l'AND tra k e 0x1f(0001 1111) e lo uso perchè così ogni numero a 8 bit (ASCII quindi) ha i primi 3 bit
//azzerati, questo mi permetto di usare i codici di controllo(che vanno da 0 a 31) senza fatica richiamando la macro
//e dandole in pasto un char

enum arrows
{
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	PAGE_UP,
	PAGE_DOWN
};

/***defines end***/

/***data start***/

struct editorConfig 
{
	int cx, cy; //cursor position x,y
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
	write(STDOUT_FILENO, "\x1b[2J", 4);//\x1b è visto come un solo byte perchè viene interpretato come "27" ovver l'escape sequence
  	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);//funziona perchè const char *s è il primo
			 //carattere della stringa che "attiva" perror
	exit(1);
	
}

void returnCookedMode()
{
	  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
	  	 //dopo di tutto tcsetattr fa TCSAFLUSH, prima scrive &E.orig_termios dentro STDIN_FILENO
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

int readKey(void)
{
	int check;
	char c;

	while((check = read(STDIN_FILENO, &c, 1)) != 1)//finchè non gli passo più di un char
	//avendo disattivato ICANON sarebbe anomalo avere più byte passati alla func
	{
		if(check == -1 && errno != EAGAIN)
			error("read");
	}

	if(c == '\x1b')
	{
		char seq[3];

		if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if(seq[0] == '[')
		{
			if(seq[1] >= '0' && seq[1] <= 9)
			{
				if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';

				if(seq[2]=='~')
				{
					switch(seq[1])
					{
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
							
					}
				}
			}
			else
			{
				switch(seq[1])
				{
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
				}
			}	
		}
		return '\x1b';
	}
	else
	{
		return c;
	}
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
    	  if (read(STDIN_FILENO, &buf[i], 1) != 1)//leggo dentro a buf attraverso il suo indirizzo
			break;

        if (buf[i] == 'R') 
        	break;

        i++;
    }

    buf[i] = '\0';

    if (buf[1] == '[')
    	 return -1;

    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) 
    //sscanf() sta per "String Scan Formatted". Questa funzione è usata per analizzare le stringhe formattate 
    //a differenza di scanf() che legge dal flusso di input, la funzione sscanf() estrae i dati da una stringa
    	return -1;

    return 0;

}

int getWindowSize(int *rows, int *cols) 
{
    struct winsize ws;
    
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) 
	{						//get win size
    	if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 10) != 10) return -1;
    	//B = cursod down C = Cursor Forward
    } 
    else 
    {
      *cols = ws.ws_col;
      *rows = ws.ws_row;

  	}
      return 0;
}


/*** Buffer ***/

struct tBuf//buffer per fare tutti i write() delle tilde insieme, invece che una alla volta
{
	char *buf;
	int lenght;
};

#define TBUF_INIT {NULL,0} //inizializzo il buffer

void makeBuf(struct tBuf *newBuf, const char *string, int bufLen)//buffer creato per disegnare le tilde
{
	char *new = realloc(newBuf->buf, newBuf->lenght + bufLen);//new è l'oggetto di grandezza l+bl

	if(new == NULL)
	{	
		return;
	}

	memcpy(&new[newBuf->lenght], string, bufLen);//Questo è un puntatore alla locazione di memoria successiva ai dati dentro "new" correnti, dire *(new + newBuf->lenght)
	newBuf->buf = new;
	newBuf->lenght += bufLen; 
}

void freeBuf(struct tBuf *freeBuff)
{
	free(freeBuff->buf);
}
/*** End Buffer ***/

/*** terminal end ***/

/*** UI start ***/

void drawTilde(struct tBuf *tildeBuff) 
{
    int y;
    
    for (y = 0; y < E.screenrows; y++) 
    {
    	if(y == 1)
    	{
    		char ciao[80];
			int ciaoLen = snprintf(ciao, sizeof(ciao), "TEIC version: %s", TEIC_VERSION);
			//In C, snprintf() function is a standard library function that is used to print the specified string till a specified length in the specified format
			//creo un buffer di caratteri dove metto "TEIC version..."

			if(ciaoLen > E.screencols)
			{
				ciaoLen = E.screencols;
			} 

			int padding = (E.screencols - ciaoLen) / 2;

			if(padding)
			{
				makeBuf(tildeBuff, "~ ", 1);
				padding--;
			}
	    	while(padding--)
	    	//prendo padding-- perchè si deve aggiornare ancora il risultato nel loop 
	    	//nel mentre disegna lo spazio e poi il titolo
	    	{
	    		makeBuf(tildeBuff, " ", 1);
	    	}

			makeBuf(tildeBuff, ciao, ciaoLen);
		}
		else
		{
    		makeBuf(tildeBuff, "~", 1);
    	}

		makeBuf(tildeBuff, "\x1b[K",3);
		//K lo usiamo per pulire lo schermo una riga alla volta	
		
      	if(y < E.screenrows - 1)
      	{
      		makeBuf(tildeBuff, "\r\n", 2);
      	}
    }
}

void renderUI()
{
	struct tBuf buff = TBUF_INIT;

	makeBuf(&buff, "\x1b[?25l",6);//?25 = nasconde/mostra cursore l = set mode
	//makeBuf(&buff, "\x1b[2J",4);
	makeBuf(&buff, "\x1b[H",3);

	
	drawTilde(&buff);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);//passiamo a %H c.xe c.y
	makeBuf(&buff, buf, strlen(buf));
	
	makeBuf(&buff, "\x1b[?25h",6);//h = reset mode
	
	write(STDOUT_FILENO,buff.buf, buff.lenght);
	//passiamo 4 byte in memoria:
	//1 byte: \x1b, che equivale all'escale character 27
	//2 byte: [, insieme a \x1b formano una escape sequence, ovvero diciamo al terminale come formattare il testo
	//3+4 byte: 2J, pulisce lo schermo(J) per intero(2)
	//H riposizione il cursore in 1;1 nel temrinale

	freeBuf(&buff);
}

/***UI end***/

/*** input start ***/

void moveCursor(int key)
{
	switch(key)
	{
		case ARROW_LEFT:
		if(E.cx != 0)	
		{
			E.cx--;
		}
		break;
	
		case ARROW_RIGHT:
			if(E.cx != E.screencols -1)
			{
				E.cx++;
			}
		break;
		
		case ARROW_UP:
			if(E.cy != 0)	
			{
				E.cy--;
			}
		break;
		
		case ARROW_DOWN:
			if(E.cy != E.screenrows -1)
			{
				E.cy++;
			}		
		break;
							
	}
}

void keypress()
{
	int cPressed = readKey();	

	switch(cPressed)
	{
		case CTRL_KEY('q'):

  			write(STDOUT_FILENO, "\x1b[2J", 4);
  			write(STDOUT_FILENO, "\x1b[H", 3);

			exit(0);//usciamo quando il valore letto è pari al valore di q(01110001) 
									//"ANDato" con 0x1f(0001 1111) che quindi diventa ctrl+q
		break;
	
		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = E.screenrows;
				while(times--)
				{
					moveCursor(cPressed == PAGE_UP ? ARROW_UP : ARROW_DOWN);
				}
			}
			break;
		
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			moveCursor(cPressed);
		break;
	
	}
}

/*** input end ***/

/*** initialization ***/

void initEditor() 
{
	E.cx = 0, E.cy = 0;


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
