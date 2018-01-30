class AO {
		
public:;
	
		AO();	
	int	Init();
	int	Send(const char *);
	void	Center();
	void	Set(int x, int y);
	void    Bump(int dx, int dy);
private:
	int	ao_fd;
	int	xpos;
	int	ypos;
};


