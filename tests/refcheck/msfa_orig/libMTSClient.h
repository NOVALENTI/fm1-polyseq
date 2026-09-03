// tests/refcheck/msfa_orig/libMTSClient.h — minimal declarations mirroring
// Dexed's vendored MTS-ESP libMTSClient.h (typedef + the two functions the
// msfa core references). The bit-exact tests always pass a null client,
// for which the real implementation is null-safe (returns false / equal
// temperament), so these curator stubs are behaviorally identical here.
#ifndef REFCHECK_LIBMTSCLIENT_H
#define REFCHECK_LIBMTSCLIENT_H

typedef struct MTSClient MTSClient;

#ifdef __cplusplus
extern "C" {
#endif

extern bool MTS_HasMaster(MTSClient *client);
extern double MTS_NoteToFrequency(MTSClient *client, char midinote,
                                  char midichannel);

#ifdef __cplusplus
}
#endif

#endif
