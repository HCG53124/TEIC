#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

struct termios original;

void error(const char *s)//handlign del messaggio di errore
{
	perror(s);
	exit(1);
	
}

void resetCookedMode()
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

	//***LISTA FLAG***//

	//ICANON serve per leggere byte per byte e non stringhe intere, disattivo, quindi, la canonical mode.
	//ISIG disattiva ctrl-c/z/ che equivalgono al terminale il processo corrente, il primo, e sospenderlo
	//il secondo
	//IEXTEN disattiva ctrl-v non pasando in input caratteri scritti dopo averlo premuto
	//IXON disattiva ctrl-s/q che usiamo per sospendere e riprendere processi
	//ICRNL disattiva ctrl-m che riporta a capo dando in input quello che c'è scritto(carraige return on newline)
	//OPOST si occupa di disattivare la funzione automtica del terminale \r che riporta all'inizio 
	//della riga il cursore

	//***FINE LISTA FLAG***//

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) 
		error("tcsetattr");
	//flusho cos'ì tutto quello inserito prima della raw mode non verrà letto
											
	atexit(resetCookedMode);//viene chiamata alla fine dell'esecuzione del programma
}

int main()
{
	rawMode();
	
	char readC;

	while(1)
	{
		if (readC == 'q') break;

		if (read(STDIN_FILENO, &readC, 1) == -1 && errno != EAGAIN) 
			error("read");

		if(iscntrl(readC))//vediamo se readC è un carattere di controllo, ovvero non stampabile
		{
			printf("%d\r\n",readC);
		}
		else
		{
			printf("%d('%c')\r\n",readC,readC);
		}//implementiamo la funzione \r disattavata in rawMode() con raw.c_oflag &= ~(OPOST);


	}
	//leggiamo da readC 1 byte alla volta
	//dalla stream di input che si crea 
	//automaticamente quando avviamo il programma
	//quando premo invio tutti i caratteri vengono letti 
	//e se c'è una 'q' il programma esce e tutto quello
	//dopo la 'q' viene mandato al terminale.
														
	return 0;
}
