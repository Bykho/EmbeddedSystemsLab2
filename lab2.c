/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Please Changeto Yourname (pcy2301)
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);


// ADDING FUNCTIONS HERE

#define TOTAL_COLS 64
#define SEPARATOR_ROW 15


void draw_separator() {
  for (int col = 0; col < TOTAL_COLS; col++) {
    fbputchar('-', SEPARATOR_ROW, col);
    }
}

void ascii_convert(int modifiers, int keycode0, int keycode1) {
  if (modifiers == 2) {
    printf("The following letter is Capital\n");
  } else {
    printf("The following letter is Lowercase\n");
  }
  printf("hex val: %02x\n", keycode0);
  printf("Ascii val - keycode0: %d\n", 'a' - keycode0);
  printf("Ascii val + keycode0: %d\n", 'a' + keycode0);
  printf("Ascii val 97 + keycode0: %d\n", 97+keycode0);
  printf("Ascii val 93 + keycode0: %d\n", 93+keycode0);
  printf("\n");
  char l = (char)(97 + keycode0); // Assuming keycode0 is in the range for lowercase letters
  printf("The 97 letter is: %c\n", l);
  char l = (char)(93 + keycode0); // Assuming keycode0 is in the range for lowercase letters
  printf("The 93 letter is: %c\n", l);
  
}

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('*', 23, col);
  }

  fbputs("Lab 2 Chat Server", 4, 10);
  
  draw_separator();
  
  /* Open the keyboard */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }
    
  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  /* Look for and handle keypresses */
  for (;;) {
    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);



    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
	      packet.keycode[1]);
      fbputs(keystate, 21, 0);
      ascii_convert(packet.modifiers, packet.keycode[0], packet.keycode[1]);
      if (packet.keycode[0] == 0x29) { /* ESC pressed? */
	break;
      }
    }
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;
}

//NICO PUSHED HERE
//HERE IS WHERE WE WRITE THE CODE THAT SHOULD MANAGE INCOMING TRAFFIC AND DISPLAY IN TOP PORTION

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  //Current line should do something like 
  //allow us to choose where the messages get written to.
  int current_line = 8;

  //IN THIS WHILE LOOP WE ARE RECIEVING DATA
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    //check if were at the bottom, is yes clear the whole recieved text
    if(current_line > SEPARATOR_ROW - 1) {
      current_line = 8;
      for(int i = 8; i < SEPARATOR_ROW; i++) {
        for(int j = 0; j < TOTAL_COLS; j++) {
          fbputs(" ", i, j);
        }
      }
    }

    recvBuf[n] = '\0';

    // Initialize line_length to 0 and tokenize the received buffer by newline characters so that we go down by lines if we have that
    int line_length = 0;
    char *token = strtok(recvBuf, "\n");
    while (token != NULL) {
      // Calculate the length of the current line (which is the token)
      line_length = strlen(token);
      // If the line is longer than the total columns, wrap it so we dont go passed the right barrier of the screen
      if (line_length > TOTAL_COLS) {
        for (int i = 0; i < line_length; i += TOTAL_COLS) {
          char line[TOTAL_COLS + 1];
          // Copy a chunk of the line into a temporary buffer
          strncpy(line, token + i, TOTAL_COLS);
          line[TOTAL_COLS] = '\0';
          // Print and display the wrapped line
          printf("%s\n", line);
          fbputs(line, current_line++, 0);
        }
      } else {
        // If the line is not too long, just go ahead and print it direct
        printf("%s\n", token);
        fbputs(token, current_line++, 0);
      }
      // Move to the next line
      token = strtok(NULL, "\n");
    }
  }
  return NULL;
}