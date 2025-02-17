#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <optional> // Disponível a partir do C++17
#include <set>
#include <cstdint>   // uint32_t
#include <limits>    // std::numeric_limits
#include <algorithm> // std::clamp

// Define a largura e altura da janela
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// Vetor que contém o nome da camada de validação que será usada
// "VK_LAYER_KHRONOS_validation" é a camada padrão de validação
const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
// Vetor que contém o nome das extensões de dispositivo que serão usadas
const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// Se o modo de debug (NDEBUG) não estiver definido, habilita as camadas de validação
// Caso contrário, mantém desabilitado
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Função auxiliar para criar o mensageiro de debug (Debug Utils Messenger)
// O Vulkan não oferece diretamente esta função (é uma extensão)
// então é necessário obter seu endereço em tempo de execução via vkGetInstanceProcAddr
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    // Obtém a função vkCreateDebugUtilsMessengerEXT usando vkGetInstanceProcAddr
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    // Se essa função estiver disponível, invoca para criar o mensageiro de debug
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        // Retorna erro caso a extensão não esteja presente
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// Função auxiliar para destruir o mensageiro de debug (Debug Utils Messenger)
// Também é obtida via vkGetInstanceProcAddr por ser uma extensão
void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    // Se a função estiver disponível, invoca para destruir o mensageiro de debug
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

// Estrutura que armazena os índices das famílias de fila de um dispositivo
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// Estrutura que armazena os detalhes de suporte a cadeia de troca de um dispositivo
struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow *window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    // Inicializa a janela GLFW
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Evita a criação de um contexto OpenGL
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // Desabilita redimensionamento
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    // Inicializa o Vulkan: cria a instância e configura o mensageiro de debug (caso habilitado)
    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
    }

    // Loop principal da aplicação
    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    // Limpa os recursos alocados
    void cleanup()
    {
        // Limpa as imageViews da cadeia de troca
        for (auto imageView : swapChainImageViews)
        {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    // Cria a instância Vulkan
    void createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // Informações sobre a aplicação (opcional, mas útil para ferramentas de diagnóstico)
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // Estrutura usada para criar a instância do Vulkan
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Obtém as extensões necessárias pelo GLFW e outras, se necessário
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Se as camadas de validação estiverem habilitadas, define-as e
        // inclui o mensageiro de debug na cadeia de criação
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // Preenche a struct debugCreateInfo
            populateDebugMessengerCreateInfo(debugCreateInfo);
            // Adiciona debugCreateInfo à cadeia de criação
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        }
        else
        {
            // Se as camadas de validação não estiverem habilitadas, não inclui nenhuma
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // Cria a instância Vulkan propriamente dita
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }

    // Preenche a estrutura de criação do mensageiro de debug com níveis de severidade e tipos de mensagem
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback; // Função de callback que será invocada
    }

    // Configura o mensageiro de debug, criando-o a partir da instância se as camadas estiverem habilitadas
    void setupDebugMessenger()
    {
        if (!enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    // Cria a superfície da janela
    void createSurface()
    {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    // Escolhe o dispositivo físico que será usado para a aplicação
    void pickPhysicalDevice()
    {
        // Recupera a quantidade de dispositivos físicos disponíveis
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        // Se não houver dispositivos físicos disponíveis, lança uma exceção
        if (deviceCount == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        // Caso haja dispositivos físicos disponíveis, recupera-os
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Itera sobre os dispositivos físicos disponíveis para encontrar um adequado
        for (const auto &device : devices)
        {
            if (isDeviceSuitable(device))
            {
                physicalDevice = device;
                break;
            }
        }

        // Se não houver um dispositivo físico adequado, lança uma exceção
        if (physicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    // Cria o dispositivo lógico a partir do dispositivo físico
    void createLogicalDevice()
    {
        // Busca as famílias de fila suportadas pelo dispositivo
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // Informa a fila de gráficos ao dispositivo
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Informa as features que o dispositivo suporta
        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        // Informa a quantidade de filas
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        // Informa a fila de gráficos ao dispositivo
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        // Informa as features que o dispositivo suporta
        createInfo.pEnabledFeatures = &deviceFeatures;

        // Informa as extensões que o dispositivo suporta
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); // Número de extensões
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();                      // Nomes das extensões

        // Se as camadas de validação estiverem habilitadas, inclui-as na cadeia de criação
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            // Caso contrário, não inclui nenhuma camada
            createInfo.enabledLayerCount = 0;
        }

        // Cria o dispositivo lógico
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create logical device!");
        }

        // Recupera a fila de gráficos do dispositivo
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        // Recupera a fila de apresentação do dispositivo
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    // Cria a cadeia de troca
    void createSwapChain()
    {
        // Consulta as capacidades de suporte da cadeia de troca do dispositivo
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        // Escolhe o formato da superfície da cadeia de troca
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // O número de imagens na cadeia de troca é uma das capacidades suportadas pelo dispositivo
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // Preenche a estrutura de criação da cadeia de troca
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        // Informa as características da cadeia de troca
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Obtém as famílias de fila suportadas pelo dispositivo
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily)
        {
            // Se as famílias de fila de gráficos e apresentação forem diferentes, a cadeia de troca suporta múltiplas filas
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            // Se as famílias de fila de gráficos e apresentação forem iguais, a cadeia de troca suporta apenas uma fila
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        // Informa a transformação da imagem
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        // Informa a cadeia de troca antiga
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // Cria a cadeia de troca
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        // Recupera as imagens da cadeia de troca
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        // Armazena o formato da imagem, a extensão e a cadeia de troca
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    // Consulta as capacidades de suporte da cadeia de troca do dispositivo
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
    {
        // Estrutura que armazena os detalhes de suporte da cadeia de troca
        SwapChainSupportDetails details;

        // Essa função leva em consideração o VkPhysicalDevice especificado e a superfície da janela VkSurfaceKHR ao determinar as capacidades suportadas
        // Todas as funções de consulta de suporte têm esses dois como primeiros parâmetros porque são os componentes principais da cadeia de troca
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // Consultar os formatos de superfície suportados
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        // Se houver formatos de superfície suportados, armazena-os em details.formats
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        // Consultar os modos de apresentação suportados
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    // Cria as imageViews da cadeia de troca
    void createImageViews()
    {
        // Redimensiona o vetor de imageViews para armazenar todas as imagens da cadeia de troca
        swapChainImageViews.resize(swapChainImages.size());

        // Itera sobre as imagens da cadeia de troca e cria uma imageView para cada uma
        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            // Cria a estrutura de criação da imageView
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            // Se a criação da imageView falhar, lança uma exceção            
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
            {   
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    // Escolhe o formato da superfície da cadeia de troca
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
    {
        // Itera sobre os formatos disponíveis e escolhe o formato B8G8R8A8 SRGB se disponível
        for (const auto &availableFormat : availableFormats)
        {
            // Escolhe o formato B8G8R8A8 SRGB e o espaço de cores SRGB não linear
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    // Escolhe o modo de apresentação da cadeia de troca
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
    {
        // Itera sobre os modos de apresentação disponíveis e escolhe o modo MAILBOX se disponível
        for (const auto &availablePresentMode : availablePresentModes)
        {
            // O modo MAILBOX é o melhor para evitar tearing
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // Escolhe a extensão da cadeia de troca
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
    {
        // Se a largura e altura da cadeia de troca forem diferentes de std::numeric_limits<uint32_t>::max(), retorna as dimensões atuais
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else // Caso contrário, ajusta as dimensões da cadeia de troca para se ajustar à janela
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)};

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    // Verifica se o dispositivo é adequado para a aplicação
    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        // Indices das famílias de fila suportadas pelo dispositivo
        QueueFamilyIndices indices = findQueueFamilies(device);

        // Verifica se o dispositivo suporta as extensões necessárias
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // Verifica se o dispositivo suporta a cadeia de troca
        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            // Consulta as capacidades de suporte da cadeia de troca do dispositivo
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        // Recupera o número de extensões de dispositivo disponíveis
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        // Recupera as extensões de dispositivo disponíveis
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // Cria um conjunto com as extensões necessárias
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        // Remove as extensões disponíveis do conjunto de extensões necessárias
        for (const auto &extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    // Encontra as famílias de fila suportadas pelo dispositivo
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }

            i++;
        }
        return indices;
    }

    // Retorna as extensões necessárias para criar a instância
    std::vector<const char *> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    // Verifica se as camadas de validação solicitadas estão disponíveis na instância Vulkan
    bool checkValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Verifica se cada camada do vetor validationLayers existe entre as camadas disponíveis
        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    // Função de callback que será chamada quando houver mensagens de validação ou erro do Vulkan
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
    {
        // Escreve a mensagem de validação/erro no stderr
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        // Retornar VK_FALSE indica que não está sendo requisitada nenhuma ação adicional
        return VK_FALSE;
    }
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}