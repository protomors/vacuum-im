#ifndef DEF_INTERNALERRORS_H
#define DEF_INTERNALERRORS_H

// BitsOfBinary
#define IERR_BOB_DATA_LOAD_ERROR                             "bob-data-load-error"
#define IERR_BOB_DATA_SAVE_ERROR                             "bob-data-save-error"
#define IERR_BOB_INVALID_RESPONCE                            "bob-invalid-responce"

// ConnectionManager
#define IERR_CONNECTIONMANAGER_CONNECT_ERROR                 "connectionmanager-connect-error"

// DefaultConnection
#define IERR_DEFAULTCONNECTION_CERT_NOT_TRUSTED              "defaultconnection-cert-not-trusted"

// DataForms
#define IERR_DATAFORMS_MEDIA_INVALID_TYPE                    "dataforms-media-invalid-type"
#define IERR_DATAFORMS_MEDIA_INVALID_FORMAT                  "dataforms-media-invalid-format"
#define IERR_DATAFORMS_URL_INVALID_SCHEME                    "dataforms-url-invalid-scheme"
#define IERR_DATAFORMS_URL_NETWORK_ERROR                     "dataforms-url-network-error"

// FileMessageArchive
#define IERR_FILEARCHIVE_DATABASE_NOT_CREATED                "filearchive-database-not-created"
#define IERR_FILEARCHIVE_DATABASE_NOT_OPENED                 "filearchive-database-not-opened"
#define IERR_FILEARCHIVE_DATABASE_NOT_COMPATIBLE             "filearchive-database-not-compatible"
#define IERR_FILEARCHIVE_DATABASE_EXEC_FAILED                "filearchive-database-exec-failed"

// HttpUpload
#define IERR_HTTPUPLOAD_NOT_SUPPORTED                        "httpupload-not-supported"
#define IERR_HTTPUPLOAD_FILE_NOT_READABLE                    "httpupload-file-not-readable"
#define IERR_HTTPUPLOAD_FILE_TOO_LARGE                       "httpupload-file-too-large"
#define IERR_HTTPUPLOAD_INVALID_SLOT                         "httpupload-invalid-slot"
#define IERR_HTTPUPLOAD_UPLOAD_FAILED                        "httpupload-upload-failed"
#define IERR_HTTPUPLOAD_ABORTED                              "httpupload-aborted"
#define IERR_HTTPUPLOAD_SEND_FAILED                          "httpupload-send-failed"

// MessageArchive
#define IERR_HISTORY_HEADERS_LOAD_ERROR                      "history-headers-load-error"
#define IERR_HISTORY_CONVERSATION_SAVE_ERROR                 "history-conversation-save-error"
#define IERR_HISTORY_CONVERSATION_LOAD_ERROR                 "history-conversation-load-error"
#define IERR_HISTORY_CONVERSATION_REMOVE_ERROR               "history-conversation-remove-error"
#define IERR_HISTORY_MODIFICATIONS_LOAD_ERROR                "history-modifications-load-error"

// Registration
#define IERR_REGISTER_UNSUPPORTED                            "register-unsupported"
#define IERR_REGISTER_INVALID_FIELDS                         "register-invalid-fields"
#define IERR_REGISTER_REJECTED_BY_USER                       "register-rejected-by-user"

// Roster
#define IERR_ROSTER_REQUEST_FAILED                           "roster-request-failed"

// SASLAuth
#define IERR_SASL_AUTH_INVALID_REQUEST                       "sasl-auth-invalid-request"
#define IERR_SASL_AUTH_INVALID_RESPONSE                      "sasl-auth-invalid-response"
#define IERR_SASL_BIND_INVALID_STREAM_JID                    "sasl-bind-invalid-stream-jid"

// StartTLS
#define IERR_STARTTLS_NOT_STARTED                            "starttls-not-started"
#define IERR_STARTTLS_INVALID_RESPONCE                       "starttls-invalid-responce"
#define IERR_STARTTLS_NEGOTIATION_FAILED                     "starttls-negotiation-failed"

// XmppStreams
#define IERR_XMPPSTREAM_DESTROYED                            "xmppstream-destroyed"
#define IERR_XMPPSTREAM_NOT_SECURE                           "xmppstream-not-secure"
#define IERR_XMPPSTREAM_CLOSED_UNEXPECTEDLY                  "xmppstream-closed-unexpectedly"
#define IERR_XMPPSTREAM_FAILED_START_CONNECTION              "xmppstream-failed-to-start-connection"
#define IERR_XMPPSTREAM_UNSUPPORTED_VERSION                  "xmppstream-unsupported-version"
#define IERR_XMPPSTREAM_RESUME_FAILED                        "xmppstream-resume-failed"

#endif // DEF_INTERNALERRORS_H
