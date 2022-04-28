//
// location.h
//
// JPG
// 
// This entire file is new in proquake.  It is used to translate map areas
// to names for the %l formatting specifier
//

typedef struct location_s
{
	struct	location_s *next_loc;
	vec3_t	mins, maxs;		
	char	name[32];
	vec_t	sum;
	float	col[3];
} location_t;

void LOC_Init (void);
// Load the locations for the current level from the location file
qboolean LOC_LoadLocations (void);
// Get the name of the location of a point
char *LOC_GetLocation (vec3_t p);
void LOC_DeleteCurrent_f (void);
void LOC_RenameCurrent_f (void);
void LOC_Clear_f (void);
void LOC_StartPoint_f (void);
void LOC_EndPoint_f (void);
void LOC_Save_f (void);

