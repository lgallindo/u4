/*
 * $Id$
 */

#ifndef CAMP_H
#define CAMP_H

#ifdef __cplusplus
extern "C" {
#endif

#define CAMP_HEAL_INTERVAL  100   /* Number of moves before camping will heal the party */

void campBegin(void);
void innBegin(void);

#ifdef __cplusplus
}
#endif

#endif
