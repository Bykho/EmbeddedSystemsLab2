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
#include <errno.h>

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
#define SEPARATOR_ROW 20
#define TEXT_ROWS 2


void draw_separator() {
  for (int col = 0; col < TOTAL_COLS; col++) {
    fbputchar('-', SEPARATOR_ROW, col);
    }
}

char ascii_convert(int modifiers, int keycode0) {
  printf("ASCII_CONVERT:ascii_convert called with modifiers: %d, keycode0: %d\n", modifiers, keycode0);
  int uppercase = 0;
  
  if (modifiers == 2 || modifiers == 32) {
    uppercase = 1;
  } else {
    uppercase = 0;
  }
  
  char l;
  // Numbers
  if (keycode0 >= 30 && keycode0 <= 39 && !uppercase) {
    if (keycode0 == 39) { // Special case for '0'
      l = '0'; // ASCII 48
    } else {
      l = (char)(19 + keycode0); // This works for 1-9
    }
  }
  // Letters
  else if (keycode0 >= 4 && keycode0 <= 29) {
    if (uppercase) {
      l = (char)(61 + keycode0);
    } else {
      l = (char)(93 + keycode0);
    }
  }
  // Handle some common special characters
  else if (keycode0 == 46) {
    l = uppercase ? '+' : '=';
  }
  else if (keycode0 == 45) {
    l = uppercase ? '_' : '-';
  }
  else if (keycode0 == 49) {
    l = uppercase ? '|' : '\\';
  }
  else if (keycode0 == 52) {
    l = uppercase ? '"' : '\'';
  }
  else if (keycode0 == 54) {
    l = uppercase ? '<' : ',';
  }
  else if (keycode0 == 55) {
    l = uppercase ? '>' : '.';
  }
  else if (keycode0 == 51) {
    l = uppercase ? ':' : ';';
  }
  else if (keycode0 == 47) {
    l = uppercase ? '{' : '[';
  }
  else if (keycode0 == 48) {
    l = uppercase ? '}' : ']';
  }
  else if (keycode0 == 53) {
    if (uppercase) {
      l = '~';
    } else {
      l = '`';
    }
  }
  else if (keycode0 == 30 && uppercase) {
    l = '!';
  }
  else if (keycode0 == 31 && uppercase) {
    l = '@';
    printf("l: %c\n", l);
  }
  else if (keycode0 == 32 && uppercase) {
    l = '#';
  }
  else if (keycode0 == 33 && uppercase) {
    l = '$';
  }
  else if (keycode0 == 34 && uppercase) {
    l = '%';
  }
  else if (keycode0 == 35 && uppercase) {
    l = '^';
  }
  else if (keycode0 == 36 && uppercase) {
    l = '&';
  }
  else if (keycode0 == 37 && uppercase) {
    l = '*';
  }
  else if (keycode0 == 38 && uppercase) {
    l = '(';
  }
  else if (keycode0 == 39 && uppercase) {
    l = ')';
  }
  else if (keycode0 == 56) {
    l = '/';
    if (uppercase) {
      l = '?';
    }
  }
  else {
    l = ' '; // Default to space for unhandled keycodes
  }
  printf("Generated character ASCII value: %d\n", (int)l);
  return l;
}

// int send_buffer_data(char ** buffer, int cols, int rows, int size)
// {
//   if (write(sockfd, (char *)buffer, size) < 0) {
//     fprintf(stderr, "Error insend_buffer_data: %s\n", strerror(errno));
//     return -1;
//   }
//   return 0;
// }

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  // char keystate[12];
  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  fbcleartop();
  fbclearbottom();

  
  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < TOTAL_COLS ; col++) {
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


  //TODO
  // 2d array to write in filled with " ". as user types we go to the next column untill we wrap around.
  // On enter send array to server.

  char textBuffer[TEXT_ROWS][TOTAL_COLS] = {{'\0'}};
  int rows = TEXT_ROWS;
  int cols = TOTAL_COLS;
  int prevRow, currentRow = SEPARATOR_ROW + 1;
  int prevCol, currentCol = 0;
  char tmp;
  int msg_len = 0; 

  // Define cursor by column and row.
  // Functions: delete, cursor left, cursor right.
  // Connect functions to key presses.

  /* Look for and handle keypresses */
  for (;;) {
    // Save the character (+ its location) that you're about to cover with the cursor.
    tmp = textBuffer[currentRow- SEPARATOR_ROW - 1][currentCol];
    printf("tmp: %c\n", tmp);
    prevRow = currentRow;
    prevCol = currentCol;

    // Cursor
    fbputchar('_', currentRow, currentCol); // Small bug: row switching is delayed. 

    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);
    if (transferred == sizeof(packet)) 
    {
      // sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
	    //   packet.keycode[1]); // we don't need this, but figure it out maybe.
      if (packet.keycode[0] == 0) 
      { // If junk. 
        continue;
      }
      if (packet.keycode[0] == 0x28) // If '/n', send message!
      { 
        if (write(sockfd, (char *)textBuffer, TOTAL_COLS*TEXT_ROWS) < 0) {
          fprintf(stderr, "Error insend_buffer_data: %s\n", strerror(errno));
          exit(1);
        }
        fbclearbottom();
        currentCol = 0;
        currentRow = SEPARATOR_ROW + 1;
        msg_len = 0;
        memset(textBuffer, 0, sizeof(textBuffer));
      }
      else if (packet.keycode[0] == 0x50) // Left arrow key pressed
      { 
        int currentAbsPos = ((currentRow - SEPARATOR_ROW - 1) * TOTAL_COLS) + currentCol;
        
        if (currentAbsPos > 0) {
            if (currentCol > 0) {
                currentCol--;
            } else if (currentRow > SEPARATOR_ROW + 1) {
                currentRow--;
                currentCol = TOTAL_COLS - 1;
            }
        }
        if (currentAbsPos != msg_len) {
          fbputchar(tmp, prevRow, prevCol);
          tmp = textBuffer[currentRow - SEPARATOR_ROW - 1][currentCol];
        }
        int final_row;
        if (msg_len > 64) {
          final_row = SEPARATOR_ROW + 2;
        } else {
          final_row = SEPARATOR_ROW + 1;
        }
        fbputchar(' ', final_row , msg_len % TOTAL_COLS);
      } 
      else if (packet.keycode[0] == 0x4f) // Right arrow key pressed
      { 
        int currentAbsPos = ((currentRow - SEPARATOR_ROW - 1) * TOTAL_COLS) + currentCol;
        
        if (currentAbsPos < msg_len) {
            if (currentCol >= TOTAL_COLS - 1) {
                currentRow++;
                currentCol = 0;
            } else {
                currentCol++;
            }
        }
        fbputchar(tmp, prevRow, prevCol);
        tmp = textBuffer[currentRow - SEPARATOR_ROW - 1][currentCol];
      }
      else if (packet.keycode[0] == 0x2a) // Backspace pressed
      {
        int currentAbsPos = ((currentRow - SEPARATOR_ROW - 1) * TOTAL_COLS) + currentCol; // absolute positon of cursor
        
        if (currentAbsPos > 0 && msg_len > 0) { // yes this would be true
            printf("beginning of backspace. current AbsPos: %d, msg_len: %d, currentCol: %d, tmp: %c\n", currentAbsPos, msg_len, currentCol, tmp);
            if (currentAbsPos == msg_len) {
                fbputchar(' ', currentRow, currentCol); // this does not seem to be working. 
            }
            
            if (currentCol > 0) {
                currentCol--;
            } else if (currentRow > SEPARATOR_ROW + 1) {
                currentRow--;
                currentCol = TOTAL_COLS - 1;
            }
            currentAbsPos--;
            
            for (int i = currentAbsPos; i < msg_len - 1; i++) {
                int row = (i / TOTAL_COLS) + SEPARATOR_ROW + 1;
                int col = i % TOTAL_COLS;
                int nextRow = ((i+1) / TOTAL_COLS) + SEPARATOR_ROW + 1;
                int nextCol = (i+1) % TOTAL_COLS;
                
                textBuffer[row - SEPARATOR_ROW - 1][col] = textBuffer[nextRow - SEPARATOR_ROW - 1][nextCol];
                fbputchar(textBuffer[row - SEPARATOR_ROW - 1][col], row, col);
            }
            
            int lastRow = ((msg_len-1) / TOTAL_COLS) + SEPARATOR_ROW + 1;
            int lastCol = (msg_len-1) % TOTAL_COLS;
            textBuffer[lastRow - SEPARATOR_ROW - 1][lastCol] = ' ';
            fbputchar(' ', lastRow, lastCol);
            
            msg_len--;
            if (currentAbsPos < msg_len) {
                fbputchar(tmp, prevRow, prevCol);
                tmp = textBuffer[currentRow - SEPARATOR_ROW - 1][currentCol];
            } else {
                fbputchar(' ', currentRow, currentCol);
                tmp = ' ';
            }
            printf("end of backspace. current AbsPos: %d, msg_len: %d, currentCol: %d, tmp: %c\n", currentAbsPos, msg_len, currentCol, tmp);
        }
      }
      else // Normal text character inputted
      { 
        if (msg_len == TEXT_ROWS * TOTAL_COLS - 1) {
          continue;
        }
        
        char l = ascii_convert(packet.modifiers, packet.keycode[0]);
        
        // If cursor is in the middle of text, shift everything right
        // Calculate absolute position in buffer based on current row and column
        int currentAbsPos = ((currentRow - SEPARATOR_ROW - 1) * TOTAL_COLS) + currentCol;
        
        // If cursor is in the middle of text, shift everything right
        if (currentAbsPos < msg_len) {
            // Shift all characters from cursor position to end of message
            for (int i = msg_len; i > currentAbsPos; i--) {
                int row = (i / TOTAL_COLS) + SEPARATOR_ROW + 1;
                int col = i % TOTAL_COLS;
                int prevRow = ((i-1) / TOTAL_COLS) + SEPARATOR_ROW + 1;
                int prevCol = (i-1) % TOTAL_COLS;
                
                textBuffer[row - SEPARATOR_ROW - 1][col] = textBuffer[prevRow - SEPARATOR_ROW - 1][prevCol];
                fbputchar(textBuffer[row - SEPARATOR_ROW - 1][col], row, col);
            }
        }
        
        textBuffer[currentRow - SEPARATOR_ROW - 1][currentCol] = l;
        fbputchar(l, currentRow, currentCol);
        
        // Update position and length
        msg_len++;
        int newAbsPos = currentAbsPos + 1;
        if (newAbsPos < TEXT_ROWS * TOTAL_COLS) {  // Check if we have room
            currentRow = (newAbsPos / TOTAL_COLS) + SEPARATOR_ROW + 1;
            currentCol = newAbsPos % TOTAL_COLS;
        }
      } 
      fbputchar('_', currentRow, currentCol);



      // FOR DEBUGGING:
      printf("about to print textBuffer: \n");
      for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
          printf("%c", textBuffer[i][j]);
        }
        printf("\n");
      }
      //fbputs(&l, currentRow, currentCol++);


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