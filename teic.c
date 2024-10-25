#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

struct termios original;

void resetCookedMode()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
	//gli sto dicendo "prendi il terminale corrente(STDIN_FILENO) e dagli queste impostazioni e poi cancella tutto
	//l'input che rimane"	
}

void rawMode()
{
	tcgetattr(STDIN_FILENO, &original);//passo a original il metodo di input del terminale

	struct termios raw = original;

	raw.c_lflag &= ~(ECHO | ICANON);
	//faccio un and e poi un not sui flag delle impostazioni del terminale per settare solo
	//e soltanto ECHO a off facendo in modo da non far vedere l'input
	//ICANON serve per leggere byte per byte e non stringhe intere, disattivo, quindi, la canonical mode.

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	//aggiorno qui le impostazioni del terminale passandogli le info dentro 
	//raw, gli dico sempre che l'input arriverà dalla stessa fonte e flusho
	//tutto il testo che è stato immesso prima dell'avio di questa funzione,
	//per questa la metto come prima cosa, così solo quello che passerò al
	//main sarà letto.
											
	atexit(resetCookedMode);//viene chiamata alla fine dell'esecuzione del programma
}

int main()
{
	rawMode();
	
	char readC;

	while(read(STDIN_FILENO, &readC, 1) && readC != 'q');
	//leggiamo da readC 1 byte alla volta
	//dalla stream di input che si crea 
	//automaticamente quando avviamo il programma
	//quando premo invio tutti i caratteri vengono letti 
	//e se c'è una 'q' il programma esce e tutto quello
	//dopo la 'q' viene mandato al terminale.
														
	return 0;
}
