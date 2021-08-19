#include <sys/kern_return.h>
#include <sys/port.h>
#include <sys/message.h>

#import "simple_types.h"

/*
 * Functions to call for handling messages returned
 */
typedef struct {
	void *		arg;		// argument to pass to function
	int		timeout;	// timeout for RPC return msg_send
	kern_return_t	(*puts)(
				void *		arg,
				simple_msg_t	string);
	kern_return_t	(*vers)(
				void *		arg,
				simple_msg_t	string);
} simple_t;

/*
 * Sizes of messages structures for send and receive.
 */
union simple_request {
	struct {
		msg_header_t Head;
		msg_type_long_t stringType;
		simple_msg_t string;
	} puts;
	struct {
		msg_header_t Head;
	} vers;
};
#define SIMPLE_INMSG_SIZE sizeof(union simple_request)

union simple_reply {
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
	} puts;
	struct {
		msg_header_t Head;
		msg_type_t RetCodeType;
		kern_return_t RetCode;
		msg_type_long_t stringType;
		simple_msg_t string;
	} vers;
};
#define SIMPLE_OUTMSG_SIZE sizeof(union simple_reply)

/*
 * Handler routine to call when receiving messages from midi driver.
 */
kern_return_t simple_handler (
	msg_header_t *msg,
	simple_t *simple);


