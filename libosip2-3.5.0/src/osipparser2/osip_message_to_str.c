/*
  The oSIP library implements the Session Initiation Protocol (SIP -rfc3261-)
  Copyright (C) 2001,2002,2003,2004,2005,2006,2007 Aymeric MOIZARD jack@atosc.org
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <stdio.h>
#include <stdlib.h>

#include <osipparser2/osip_port.h>
#include <osipparser2/osip_parser.h>

#define MIME_MAX_BOUNDARY_LEN 70

extern const char *osip_protocol_version;

static int strcat_simple_header(char **_string, size_t * malloc_size,
								char **_message, void *ptr_header,
								char *header_name, size_t size_of_header,
								int (*xxx_to_str) (void *, char **), char **next);
static int strcat_headers_one_per_line(char **_string, size_t * malloc_size,
									   char **_message,
									   osip_list_t * headers, char *header,
									   size_t size_of_header,
									   int (*xxx_to_str) (void *, char **),
									   char **next);
#if 0
static int strcat_headers_all_on_one_line(char **_string,
										  size_t * malloc_size,
										  char **_message,
										  osip_list_t * headers,
										  char *header,
										  size_t size_of_header,
										  int (*xxx_to_str) (void *,
															 char **),
										  char **next);
#endif

static int __osip_message_startline_to_strreq(osip_message_t * sip, char **dest)
{
	const char *sip_version;
	char *tmp;
	char *rquri;
	int i;

	*dest = NULL;
	if ((sip == NULL) || (sip->req_uri == NULL) || (sip->sip_method == NULL))
		return OSIP_BADPARAMETER;

	i = osip_uri_to_str(sip->req_uri, &rquri);
	if (i != 0)
		return i;

	if (sip->sip_version == NULL)
		sip_version = osip_protocol_version;
	else
		sip_version = sip->sip_version;

	if (NULL != sip->v3_head)
	{
		*dest = (char *) osip_malloc(strlen(sip->sip_method)
								 + strlen(sip->v3_head) + strlen(rquri) + strlen(sip_version) + 3);
	}
	else
	{
		*dest = (char *) osip_malloc(strlen(sip->sip_method)
								 + strlen(rquri) + strlen(sip_version) + 3);
	}
	
	
	/* *dest = (char *) osip_malloc(strlen(sip->sip_method)
								 + strlen(rquri) + strlen(sip_version) + 3); */
	if (*dest == NULL) {
		osip_free(rquri);
		return OSIP_NOMEM;
	}
	tmp = *dest;
	tmp = osip_str_append(tmp, sip->sip_method);
	*tmp = ' ';
	tmp++;
	tmp = osip_str_append(tmp, rquri);
	
	if(NULL != sip->v3_head)
	{
		tmp = osip_str_append(tmp, sip->v3_head);
	}
	
	*tmp = ' ';
	tmp++;
	strcpy(tmp, sip_version);
	osip_free(rquri);
	return OSIP_SUCCESS;
}

static int __osip_message_startline_to_strresp(osip_message_t * sip, char **dest)
{
	char *tmp;
	const char *sip_version;
	char status_code[5];

	*dest = NULL;
	if ((sip == NULL) || (sip->reason_phrase == NULL)
		|| (sip->status_code < 100) || (sip->status_code > 699))
		return OSIP_BADPARAMETER;

	if (sip->sip_version == NULL)
		sip_version = osip_protocol_version;
	else
		sip_version = sip->sip_version;

	sprintf(status_code, "%u", sip->status_code);

	*dest = (char *) osip_malloc(strlen(sip_version)
								 + 3 + strlen(sip->reason_phrase) + 4);
	if (*dest == NULL)
		return OSIP_NOMEM;
	tmp = *dest;

	tmp = osip_str_append(tmp, sip_version);
	*tmp = ' ';
	tmp++;

	tmp = osip_strn_append(tmp, status_code, 3);
	*tmp = ' ';
	tmp++;
	strcpy(tmp, sip->reason_phrase);

	return OSIP_SUCCESS;
}

static int __osip_message_startline_to_str(osip_message_t * sip, char **dest)
{

	if (sip->sip_method != NULL)
		return __osip_message_startline_to_strreq(sip, dest);
	if (sip->status_code != 0)
		return __osip_message_startline_to_strresp(sip, dest);

	OSIP_TRACE(osip_trace
			   (__FILE__, __LINE__, TRACE_LEVEL1, NULL,
				"ERROR method has no value or status code is 0!\n"));
	return OSIP_BADPARAMETER;	/* should never come here */
}

char *osip_message_get_reason_phrase(const osip_message_t * sip)
{
	return sip->reason_phrase;
}

int osip_message_get_status_code(const osip_message_t * sip)
{
	return sip->status_code;
}

char *osip_message_get_method(const osip_message_t * sip)
{
	return sip->sip_method;
}

char *osip_message_get_version(const osip_message_t * sip)
{
	return sip->sip_version;
}

osip_uri_t *osip_message_get_uri(const osip_message_t * sip)
{
	return sip->req_uri;
}

static int
strcat_simple_header(char **_string, size_t * malloc_size,
					 char **_message, void *ptr_header, char *header_name,
					 size_t size_of_header, int (*xxx_to_str) (void *,
															   char **),
					 char **next)
{
	char *string;
	char *message;
	char *tmp;
	int i;

	string = *_string;
	message = *_message;

	if (ptr_header != NULL) {
		if (*malloc_size < message - string + 100 + size_of_header)
			/* take some memory avoid to osip_realloc too much often */
		{						/* should not happen often */
			size_t size = message - string;

			*malloc_size = message - string + size_of_header + 100;
			string = osip_realloc(string, *malloc_size);
			if (string == NULL) {
				*_string = NULL;
				*_message = NULL;
				return OSIP_NOMEM;
			}
			message = string + size;
		}
		message = osip_strn_append(message, header_name, size_of_header);

		i = xxx_to_str(ptr_header, &tmp);
		if (i != 0) {
			*_string = string;
			*_message = message;
			*next = NULL;
			return i;
		}
		if (*malloc_size < message - string + strlen(tmp) + 100) {
			size_t size = message - string;

			*malloc_size = message - string + strlen(tmp) + 100;
			string = osip_realloc(string, *malloc_size);
			if (string == NULL) {
				*_string = NULL;
				*_message = NULL;
				return OSIP_NOMEM;
			}
			message = string + size;
		}

		message = osip_str_append(message, tmp);
		osip_free(tmp);
		message = osip_strn_append(message, CRLF, 2);
	}
	*_string = string;
	*_message = message;
	*next = message;
	return OSIP_SUCCESS;
}

static int
strcat_headers_one_per_line(char **_string, size_t * malloc_size,
							char **_message, osip_list_t * headers,
							char *header, size_t size_of_header,
							int (*xxx_to_str) (void *, char **), char **next)
{
	char *string;
	char *message;
	char *tmp;
	int pos = 0;
	int i;

	string = *_string;
	message = *_message;

	while (!osip_list_eol(headers, pos)) {
		void *elt;

		elt = (void *) osip_list_get(headers, pos);

		if (*malloc_size < message - string + 100 + size_of_header)
			/* take some memory avoid to osip_realloc too much often */
		{						/* should not happen often */
			size_t size = message - string;

			*malloc_size = message - string + size_of_header + 100;
			string = osip_realloc(string, *malloc_size);
			if (string == NULL) {
				*_string = NULL;
				*_message = NULL;
				return OSIP_NOMEM;
			}
			message = string + size;
		}
		osip_strncpy(message, header, size_of_header);
		i = xxx_to_str(elt, &tmp);
		if (i != 0) {
			*_string = string;
			*_message = message;
			*next = NULL;
			return i;
		}
		message = message + strlen(message);

		if (*malloc_size < message - string + strlen(tmp) + 100) {
			size_t size = message - string;

			*malloc_size = message - string + strlen(tmp) + 100;
			string = osip_realloc(string, *malloc_size);
			if (string == NULL) {
				*_string = NULL;
				*_message = NULL;
				return OSIP_NOMEM;
			}
			message = string + size;
		}
		message = osip_str_append(message, tmp);
		osip_free(tmp);
		message = osip_strn_append(message, CRLF, 2);
		pos++;
	}
	*_string = string;
	*_message = message;
	*next = message;
	return OSIP_SUCCESS;
}

#if 0
static int
strcat_headers_all_on_one_line(char **_string, size_t * malloc_size,
							   char **_message, osip_list_t * headers,
							   char *header, size_t size_of_header,
							   int (*xxx_to_str) (void *, char **), char **next)
{
	char *string;
	char *message;
	char *tmp;
	int pos = 0;
	int i;

	string = *_string;
	message = *_message;

	pos = 0;
	while (!osip_list_eol(headers, pos)) {
		if (*malloc_size < message - string + 100 + size_of_header)
			/* take some memory avoid to osip_realloc too much often */
		{						/* should not happen often */
			size_t size = message - string;

			*malloc_size = message - string + size_of_header + 100;
			string = osip_realloc(string, *malloc_size);
			if (string == NULL) {
				*_string = NULL;
				*_message = NULL;
				return OSIP_NOMEM;
			}
			message = string + size;
		}
		message = osip_strn_append(message, header, size_of_header);

		while (!osip_list_eol(headers, pos)) {
			void *elt;

			elt = (void *) osip_list_get(headers, pos);
			i = xxx_to_str(elt, &tmp);
			if (i != 0) {
				*_string = string;
				*_message = message;
				*next = NULL;
				return i;
			}
			if (*malloc_size < message - string + strlen(tmp) + 100) {
				size_t size = message - string;

				*malloc_size = message - string + (int) strlen(tmp) + 100;
				string = osip_realloc(string, *malloc_size);
				if (string == NULL) {
					*_string = NULL;
					*_message = NULL;
					return OSIP_NOMEM;
				}
				message = string + size;
			}

			message = osip_str_append(message, tmp);
			osip_free(tmp);

			pos++;
			if (!osip_list_eol(headers, pos)) {
				message = osip_strn_append(message, ", ", 2);
			}
		}
		message = osip_strn_append(message, CRLF, 2);
	}
	*_string = string;
	*_message = message;
	*next = message;
	return OSIP_SUCCESS;
}
#endif

 /* return values:
    1: structure and buffer "message" are identical.
    2: buffer "message" is not up to date with the structure info (call osip_message_to_str to update it).
    -1 on error.
  */
int osip_message_get__property(const osip_message_t * sip)
{
	if (sip == NULL)
		return OSIP_BADPARAMETER;
	return sip->message_property;
}

int osip_message_force_update(osip_message_t * sip)
{
	if (sip == NULL)
		return OSIP_BADPARAMETER;
	sip->message_property = 2;
	return OSIP_SUCCESS;
}

static int
_osip_message_realloc(char **message, char **dest, size_t needed,
					  size_t * malloc_size)
{
	size_t size = *message - *dest;

	if (*malloc_size < (size_t) (size + needed + 100)) {
		*malloc_size = size + needed + 100;
		*dest = osip_realloc(*dest, *malloc_size);
		if (*dest == NULL)
			return OSIP_NOMEM;
		*message = *dest + size;
	}

	return OSIP_SUCCESS;
}

static int
_osip_message_to_str(osip_message_t * sip, char **dest,
					 size_t * message_length, int sipfrag)
{
	size_t malloc_size;
	size_t total_length = 0;

	/* Added at SIPit day1 */
	char *start_of_bodies;
	char *content_length_to_modify = NULL;

	char *message;
	char *next;
	char *tmp;
	int pos;
	int i;
	char *boundary = NULL;

    int nBodyLen = 0; //待扩展字符加包体长度加上结束的#
    int nCurBodyLen = 0;
    int nEachPartLen[4] = { 0 };
    int nPartNum = 0;
    int EachPartIndex = 0;

	malloc_size = SIP_MESSAGE_MAX_LENGTH;

	*dest = NULL;
	if (sip == NULL)
		return OSIP_BADPARAMETER;

	{
		if (1 == osip_message_get__property(sip)) {	/* message is already available in "message" */

			*dest = osip_malloc(sip->message_length + 1);
			if (*dest == NULL)
				return OSIP_NOMEM;
			memcpy(*dest, sip->message, sip->message_length);
			(*dest)[sip->message_length] = '\0';
			if (message_length != NULL)
				*message_length = sip->message_length;
			return OSIP_SUCCESS;
		} else {
			/* message should be rebuilt: delete the old one if exists. */
			osip_free(sip->message);
			sip->message = NULL;
		}
	}

	message = (char *) osip_malloc(SIP_MESSAGE_MAX_LENGTH);	/* ???? message could be > 4000  */
	if (message == NULL)
		return OSIP_NOMEM;
	*dest = message;

	/* add the first line of message */
	i = __osip_message_startline_to_str(sip, &tmp);
	if (i != 0) {
		if (!sipfrag) {
			osip_free(*dest);
			*dest = NULL;
			return i;
		}

		/* A start-line isn't required for message/sipfrag parts. */
	} else {

		char pcV3HeadExt[6] = {0xfe,0xfe,0xfe,0xfe,0xfe, 0xfe};
		message = osip_strn_append(message, pcV3HeadExt, 6);
		message = osip_str_append(message, tmp);
		osip_free(tmp);
		message = osip_strn_append(message, CRLF, 2);
	}

	{
		struct to_str_table {
			char header_name[30];
			int header_length;
			osip_list_t *header_list;
			void *header_data;
			int (*to_str) (void *, char **);
		}
#ifndef MINISIZE
		table[25] =
#else
		table[15] =
#endif
		{
			{
			"Via: ", 5, NULL, NULL, (int (*)(void *, char **)) &osip_via_to_str}, {
			"Record-Route: ", 14, NULL, NULL,
					(int (*)(void *, char **)) &osip_record_route_to_str}, {
			"Route: ", 7, NULL, NULL,
					(int (*)(void *, char **)) &osip_route_to_str}, {
			"From: ", 6, NULL, NULL,
					(int (*)(void *, char **)) &osip_from_to_str}, {
			"To: ", 4, NULL, NULL, (int (*)(void *, char **)) &osip_to_to_str}, {
			"Call-ID: ", 9, NULL, NULL,
					(int (*)(void *, char **)) &osip_call_id_to_str}, {
			"CSeq: ", 6, NULL, NULL,
					(int (*)(void *, char **)) &osip_cseq_to_str}, {
			"Contact: ", 9, NULL, NULL,
					(int (*)(void *, char **)) &osip_contact_to_str}, {
			"Authorization: ", 15, NULL, NULL,
					(int (*)(void *, char **)) &osip_authorization_to_str}, {
			"WWW-Authenticate: ", 18, NULL, NULL,
					(int (*)(void *, char **)) &osip_www_authenticate_to_str}, {
			"Proxy-Authenticate: ", 20, NULL, NULL,
					(int (*)(void *, char **)) &osip_www_authenticate_to_str}, {
			"Proxy-Authorization: ", 21, NULL, NULL,
					(int (*)(void *, char **)) &osip_authorization_to_str}, {
			"Content-Type: ", 14, NULL, NULL,
					(int (*)(void *, char **)) &osip_content_type_to_str}, {
			"Mime-Version: ", 14, NULL, NULL,
					(int (*)(void *, char **)) &osip_content_length_to_str},
#ifndef MINISIZE
			{
			"Allow: ", 7, NULL, NULL,
					(int (*)(void *, char **)) &osip_allow_to_str}, {
			"Content-Encoding: ", 18, NULL, NULL,
					(int (*)(void *, char **)) &osip_content_encoding_to_str}, {
			"Call-Info: ", 11, NULL, NULL,
					(int (*)(void *, char **)) &osip_call_info_to_str}, {
			"Alert-Info: ", 12, NULL, NULL,
					(int (*)(void *, char **)) &osip_call_info_to_str}, {
			"Error-Info: ", 12, NULL, NULL,
					(int (*)(void *, char **)) &osip_call_info_to_str}, {
			"Accept: ", 8, NULL, NULL,
					(int (*)(void *, char **)) &osip_accept_to_str}, {
			"Accept-Encoding: ", 17, NULL, NULL,
					(int (*)(void *, char **)) &osip_accept_encoding_to_str}, {
			"Accept-Language: ", 17, NULL, NULL,
					(int (*)(void *, char **)) &osip_accept_language_to_str}, {
			"Authentication-Info: ", 21, NULL, NULL,
					(int (*)(void *, char **)) &osip_authentication_info_to_str}, {
			"Proxy-Authentication-Info: ", 27, NULL, NULL,
					(int (*)(void *, char **)) &osip_authentication_info_to_str},
#endif
			{ {
			'\0'}, 0, NULL, NULL, NULL}
		};
		table[0].header_list = &sip->vias;
		table[1].header_list = &sip->record_routes;
		table[2].header_list = &sip->routes;
		table[3].header_data = sip->from;
		table[4].header_data = sip->to;
		table[5].header_data = sip->call_id;
		table[6].header_data = sip->cseq;
		table[7].header_list = &sip->contacts;
		table[8].header_list = &sip->authorizations;
		table[9].header_list = &sip->www_authenticates;
		table[10].header_list = &sip->proxy_authenticates;
		table[11].header_list = &sip->proxy_authorizations;
		table[12].header_data = sip->content_type;
		table[13].header_data = sip->mime_version;
#ifndef MINISIZE
		table[14].header_list = &sip->allows;
		table[15].header_list = &sip->content_encodings;
		table[16].header_list = &sip->call_infos;
		table[17].header_list = &sip->alert_infos;
		table[18].header_list = &sip->error_infos;
		table[19].header_list = &sip->accepts;
		table[20].header_list = &sip->accept_encodings;
		table[21].header_list = &sip->accept_languages;
		table[22].header_list = &sip->authentication_infos;
		table[23].header_list = &sip->proxy_authentication_infos;
#endif

		pos = 0;
		while (table[pos].header_name[0] != '\0') {
			if (table[13].header_list == NULL)
				i = strcat_simple_header(dest, &malloc_size, &message,
										 table[pos].header_data,
										 table[pos].header_name,
										 table[pos].header_length,
										 ((int (*)(void *, char **))
										  table[pos].to_str), &next);
			i = strcat_headers_one_per_line(dest, &malloc_size, &message,
											table[pos].header_list,
											table[pos].header_name,
											table[pos].header_length,
											((int (*)(void *, char **))
											 table[pos].to_str), &next);
			if (i != 0) {
				osip_free(*dest);
				*dest = NULL;
				return i;
			}
			message = next;

			pos++;
		}
	}

	pos = 0;
	while (!osip_list_eol(&sip->headers, pos)) {
		osip_header_t *header;
		size_t header_len = 0;

		header = (osip_header_t *) osip_list_get(&sip->headers, pos);
		i = osip_header_to_str(header, &tmp);
		if (i != 0) {
			osip_free(*dest);
			*dest = NULL;
			return i;
		}

		header_len = strlen(tmp);

		if (_osip_message_realloc(&message, dest, header_len + 3, &malloc_size) <
			0) {
			osip_free(tmp);
			*dest = NULL;
			return OSIP_NOMEM;
		}

		message = osip_str_append(message, tmp);
		osip_free(tmp);
		message = osip_strn_append(message, CRLF, 2);

		pos++;
	}

	/* we have to create the body before adding the contentlength */
	/* add enough lenght for "Content-Length: " */

	if (_osip_message_realloc(&message, dest, 16, &malloc_size) < 0)
		return OSIP_NOMEM;

	if (sipfrag && osip_list_eol(&sip->bodies, 0)) {
		/* end of headers */
		osip_strncpy(message, CRLF, 2);
		message = message + 2;

		//v3结束
		message = osip_strn_append(message, "#", 1);
        
        nBodyLen = message - *dest - 6; //待扩展字符加包体长度加上结束的#
		nCurBodyLen = nBodyLen;
		//nEachPartLen[4] = { 0 };
        EachPartIndex = 0;
        for (; EachPartIndex < 4; EachPartIndex++)
        {
            nEachPartLen[EachPartIndex] = 0;
        }
		nPartNum = 0;
		for (; nCurBodyLen > 0; )
		{
			nEachPartLen[nPartNum] = nCurBodyLen % 256;
			nPartNum++;
			if (nPartNum > 4)
			{
				break;
			}
			
			nCurBodyLen = nCurBodyLen / 256;
		}


		/* same remark as at the beginning of the method */
		sip->message_property = 1;
		sip->message = osip_strdup(*dest);

		(*dest)[0] = 0x0;
		(*dest)[1] = nEachPartLen[0];
		(*dest)[2] = nEachPartLen[1];
		(*dest)[3] = nEachPartLen[2];
		(*dest)[4] = nEachPartLen[3];
		(*dest)[5] = 0x1;

		sip->message[0] = 0x0;
		sip->message[1] = nEachPartLen[0];
		sip->message[2] = nEachPartLen[1];
		sip->message[3] = nEachPartLen[2];
		sip->message[4] = nEachPartLen[3];
		sip->message[5] = 0x1;
		sip->message_length = message - *dest;
		if (message_length != NULL)
			*message_length = message - *dest;

		return OSIP_SUCCESS;	/* it's all done */
	}

	osip_strncpy(message, "Content-Length: ", 16);
	message = message + 16;

	/* SIPit Day1
	   ALWAYS RECALCULATE?
	   if (sip->contentlength!=NULL)
	   {
	   i = osip_content_length_to_str(sip->contentlength, &tmp);
	   if (i!=0) {
	   osip_free(*dest);
	   *dest = NULL;
	   return i;
	   }
	   osip_strncpy(message,tmp,strlen(tmp));
	   osip_free(tmp);
	   }
	   else
	   { */
	if (osip_list_eol(&sip->bodies, 0))	/* no body */
		message = osip_strn_append(message, "0", 1);
	else {
		/* BUG: p130 (rfc2543bis-04)
		   "No SP after last token or quoted string"

		   In fact, if extra spaces exist: the stack can't be used
		   to make user-agent that wants to make authentication...
		   This should be changed...
		 */

		content_length_to_modify = message;
		message = osip_str_append(message, "     ");
	}
	/*  } */

	message = osip_strn_append(message, CRLF, 2);


	/* end of headers */
	message = osip_strn_append(message, CRLF, 2);

	start_of_bodies = message;
	total_length = start_of_bodies - *dest;

	//tbd 没有消息体 直接加#结束
	if (osip_list_eol(&sip->bodies, 0)) {
		/* same remark as at the beginning of the method */
		//v3结束
		message = osip_strn_append(message, "#", 1);
		total_length = total_length + 1;

		//v3 头 数据设置
		nBodyLen = total_length - 6; //待扩展字符加包体长度加上结束的#
		nCurBodyLen = nBodyLen;
         EachPartIndex = 0;
        for (; EachPartIndex < 4; EachPartIndex++)
        {
            nEachPartLen[EachPartIndex] = 0;
        }
		//nEachPartLen[4] = { 0 }; 
		nPartNum = 0;
		for (; nCurBodyLen > 0; )
		{
			nEachPartLen[nPartNum] = nCurBodyLen % 256;
			nPartNum++;
			if (nPartNum > 4)
			{
				break;
			}
			
			nCurBodyLen = nCurBodyLen / 256;
		}

								
		sip->message_property = 1;
		sip->message = osip_strdup(*dest);
		(*dest)[0] = 0x0;
		(*dest)[1] = nEachPartLen[0];
		(*dest)[2] = nEachPartLen[1];
		(*dest)[3] = nEachPartLen[2];
		(*dest)[4] = nEachPartLen[3];
		(*dest)[5] = 0x1;
		sip->message[0] = 0x0;
		sip->message[1] = nEachPartLen[0];
		sip->message[2] = nEachPartLen[1];
		sip->message[3] = nEachPartLen[2];
		sip->message[4] = nEachPartLen[3];
		sip->message[5] = 0x1;
		sip->message_length = total_length;
		if (message_length != NULL)
			*message_length = total_length;
		return OSIP_SUCCESS;	/* it's all done */
	}

	if (sip->mime_version != NULL && sip->content_type
		&& sip->content_type->type
		&& !osip_strcasecmp(sip->content_type->type, "multipart")) {
		osip_generic_param_t *ct_param = NULL;

		/* find the boundary */
		i = osip_generic_param_get_byname(&sip->content_type->gen_params,
										  "boundary", &ct_param);
		if ((i >= 0) && ct_param && ct_param->gvalue) {
			size_t len = strlen(ct_param->gvalue);

			if (len > MIME_MAX_BOUNDARY_LEN) {
				osip_free(*dest);
				*dest = NULL;
				return OSIP_SYNTAXERROR;
			}

			boundary = osip_malloc(len + 5);
			if (boundary == NULL) {
				osip_free(*dest);
				*dest = NULL;
				return OSIP_NOMEM;
			}

			osip_strncpy(boundary, CRLF, 2);
			osip_strncpy(boundary + 2, "--", 2);

			if (ct_param->gvalue[0] == '"' && ct_param->gvalue[len - 1] == '"')
				osip_strncpy(boundary + 4, ct_param->gvalue + 1, len - 2);
			else
				osip_strncpy(boundary + 4, ct_param->gvalue, len);
		}
	}

	pos = 0;
	while (!osip_list_eol(&sip->bodies, pos)) {
		osip_body_t *body;
		size_t body_length;

		body = (osip_body_t *) osip_list_get(&sip->bodies, pos);

		if (boundary) {
			/* Needs at most 77 bytes,
			   last realloc allocate at least 100 bytes extra */
			message = osip_str_append(message, boundary);
			message = osip_strn_append(message, CRLF, 2);
		}

		i = osip_body_to_str(body, &tmp, &body_length);
		if (i != 0) {
			osip_free(*dest);
			*dest = NULL;
			if (boundary)
				osip_free(boundary);
			return i;
		}

		if (malloc_size < message - *dest + 100 + body_length) {
			size_t size = message - *dest;
			int offset_of_body;
			int offset_content_length_to_modify = 0;

			offset_of_body = (int) (start_of_bodies - *dest);
			if (content_length_to_modify != NULL)
				offset_content_length_to_modify =
					(int) (content_length_to_modify - *dest);
			malloc_size = message - *dest + body_length + 100;
			*dest = osip_realloc(*dest, malloc_size);
			if (*dest == NULL) {
				osip_free(tmp);	/* fixed 09/Jun/2005 */
				if (boundary)
					osip_free(boundary);
				return OSIP_NOMEM;
			}
			start_of_bodies = *dest + offset_of_body;
			if (content_length_to_modify != NULL)
				content_length_to_modify = *dest + offset_content_length_to_modify;
			message = *dest + size;
		}

		memcpy(message, tmp, body_length);
		message[body_length] = '\0';
		osip_free(tmp);
		message = message + body_length;

		pos++;
	}

	if (boundary) {
		/* Needs at most 79 bytes,
		   last realloc allocate at least 100 bytes extra */
		message = osip_str_append(message, boundary);
		message = osip_strn_append(message, "--", 2);
		message = osip_strn_append(message, CRLF, 2);

		osip_free(boundary);
		boundary = NULL;
	}

	if (content_length_to_modify == NULL) {
		osip_free(*dest);
		*dest = NULL;
		return OSIP_SYNTAXERROR;
	}

	/* we NOW have the length of bodies: */
	{
		size_t size = message - start_of_bodies;
		char tmp2[15];

		total_length += size;
		sprintf(tmp2, "%i", size);
		/* do not use osip_strncpy here! */
		strncpy(content_length_to_modify + 5 - strlen(tmp2), tmp2, strlen(tmp2));
	}

	message = osip_strn_append(message, "#", 1);
	total_length = total_length + 1;

	//v3 头 数据设置
	nBodyLen = total_length - 6; //待扩展字符加包体长度加上结束的#
	nCurBodyLen = nBodyLen;
	//int nEachPartLen[4] = { 0 };
    EachPartIndex = 0;
    for (; EachPartIndex < 4; EachPartIndex++)
    {
        nEachPartLen[EachPartIndex] = 0;
    }
	nPartNum = 0;
	for (; nCurBodyLen > 0; )
	{
		nEachPartLen[nPartNum] = nCurBodyLen % 256;
		nPartNum++;
		if (nPartNum > 4)
		{
			break;
		}
			
		nCurBodyLen = nCurBodyLen / 256;
	}


	/* same remark as at the beginning of the method */
	sip->message_property = 1;
	sip->message = osip_malloc(total_length + 1);
	if (sip->message != NULL) {
		//消息体
		memcpy(sip->message, *dest, total_length);
		//结束
		sip->message[total_length] = '\0';
		sip->message_length = total_length; 
		if (message_length != NULL)
			*message_length = total_length;
	}

	(*dest)[0] = 0x0;
	(*dest)[1] = nEachPartLen[0];
	(*dest)[2] = nEachPartLen[1];
	(*dest)[3] = nEachPartLen[2];
	(*dest)[4] = nEachPartLen[3];
	(*dest)[5] = 0x1;

	if (sip->message != NULL) {
		sip->message[0] = 0x0;
		sip->message[1] = nEachPartLen[0];
		sip->message[2] = nEachPartLen[1];
		sip->message[3] = nEachPartLen[2];
		sip->message[4] = nEachPartLen[3];
		sip->message[5] = 0x1;
	}
	return OSIP_SUCCESS;
}

int osip_message_to_str(osip_message_t * sip, char **dest, size_t * message_length)
{
	return _osip_message_to_str(sip, dest, message_length, 0);
}

int
osip_message_to_str_sipfrag(osip_message_t * sip, char **dest,
							size_t * message_length)
{
	return _osip_message_to_str(sip, dest, message_length, 1);
}
