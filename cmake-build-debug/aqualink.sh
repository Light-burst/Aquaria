cd /home/alex/Desktop/Aquaria/source/cmake-build-debug
if [ -L aquaria ];
then
	exit
fi

rm /home/alex/Desktop/Aquaria/Aquaria/aquaria
mv aquaria /home/alex/Desktop/Aquaria/Aquaria/aquaria
ln -s /home/alex/Desktop/Aquaria/Aquaria/aquaria aquaria
