#include "../renderer/IoHandler.hpp"
#include "../utils/utils.hpp"  

class GlewGlfwHandler
{
public:
	GlewGlfwHandler(glm::ivec2 windowDimensions, std::string windowName);
	~GlewGlfwHandler() {};
	int init();
	GLFWwindow* getWindow();
private:
	GLFWwindow* window;
};