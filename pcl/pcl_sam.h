
#ifndef __PCL_H_
#define __PCL_H_

void	    pcl_init(void);
char*       pcl_get_var(char *name);
int	        pcl_exec(const char *cmd, char **out);
int	        pcl_exec2(const char *cmd);
void	    pcl_write_shell(char *str);
int	        pcl_get_line_count(void);
int	        pcl_get_str(char *buf, int len, int n);
int32_t     pcl_get_chunksz(uint8_t *fptr, int32_t fsz);
void	    pcl_load(void);
void	    pcl_lock(void);
int	        pcl_try_lock(void);
void	    pcl_unlock(void);
void	    pcl_try_timer_cb(void);

#endif
