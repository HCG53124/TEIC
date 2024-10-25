/*** inludes start ***/

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

/*** inludes end ***/

/*** defines start ***/

#define CTRL_KEY(k) ((k) & 0x1f)
//faccio l'AND tra k e 0x1f(0001 1111) e lo uso perchè così ogni numero a 8 bit (ASCII quindi) ha i primi 3 bit
//azzerati, questo mi permetto di usare i codici di controllo(che vanno da 0 a 31) senza fatica richiamando la macro
//e dandole in pasto un char

/***defines end***/

/***data start***/

struct termios original;

/*** data end ***/

/*** terminal start ***/

void error(const char *s)//handlign del messaggio di errore
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
  	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
	
}

void returnCookedMode()
{
	  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) == -1) 
	  	error("tcgetattr");
	
}

void rawMode()
{
	if (tcgetattr(STDIN_FILENO, &original) == -1) 
		error("tcgetattr");

	struct termios raw = original;

	//DISABILITA FLAG PER PERMETTERE EDITING NEL TEXT EDITOR E NON NEL TERMINALE
						
	//faccio un "and" e poi un "not" sui flag delle impostazioni del terminale per settare solo
	//e soltanto i flag che voglio io a off
	
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);//si occupa del terminale

	raw.c_iflag &= ~(ICRNL | IXON);//si occupa dell'input
	raw.c_oflag &= ~(OPOST);//si occupa dell'output

	//*** LISTA FLAG ***//

		//ICANON serve per leggere byte per byte e non stringhe intere, disattivo, quindi, la canonical mode.
		//ISIG disattiva ctrl-c/z/ che equivalgono al terminale il processo corrente, il primo, e sospenderlo
		//il secondo
		//IEXTEN disattiva ctrl-v non pasando in input caratteri scritti dopo averlo premuto
		//IXON disattiva ctrl-s/q che usiamo per sospendere e riprendere processi
		//ICRNL disattiva ctrl-m che riporta a capo dando in input quello che c'è scritto(carraige return on newline)
		//OPOST si occupa di disattivare la funzione automtica del terminale \r che riporta all'inizio 
		//della riga il cursore

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

	while((check = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if(check == -1 && errno != EAGAIN)
			error("read");
	}

	return c;
}

/*** terminal end ***/

/*** UI start ***/

void renderUI()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	//passiamo 4 byte al terminale:
	//1 byte: \x1b, che equivale all'escale character 27
	//2 byte: [, insieme a \x1b formano una escape sequence, ovvero diciamo al terminale come formattare il testo
	//3+4 byte: 2J, pulisce lo schermo(J) per intero(2)
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

int main()
{
	rawMode();
	
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
