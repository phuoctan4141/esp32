#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "heat.h"

#include "model.h"
#include "http.h"
#include "wifi.h"

// Globals, used for compatibility with Arduino-style sketches.
namespace
{
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    // An area of memory to use for input, output, and intermediate arrays.
    constexpr int kTensorArenaSize = 2 * 1024;
    static uint8_t tensor_arena[kTensorArenaSize];
} // namespace

#define TASK_PERIOD 1000

static const char *TAG = "heat";

static void configure_tflite(void)
{
    // Map the model into a usable data structure. This doesn't involve any
    // copying or parsing, it's a very lightweight operation.
    model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        ESP_LOGE(TAG, "Model provided is schema version %d not equal to supported "
                      "version %d.",
                 model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    // Pull in only the operation implementations we need.
    // This relies on a complete list of all the ops needed by this graph.
    // An easier approach is to just use the AllOpsResolver, but this will
    // incur some penalty in code space for op implementations that are not
    // needed by this graph.
    //
    // tflite::AllOpsResolver resolver;
    // NOLINTNEXTLINE(runtime-global-variables)
    static tflite::MicroMutableOpResolver<1> micro_op_resolver;
    micro_op_resolver.AddFullyConnected();

    // This pulls in all the operation implementations we need.
    // NOLINTNEXTLINE(runtime-global-variables)
    static tflite::AllOpsResolver resolver;

    // Build an interpreter to run the model with.
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "AllocateTensors() failed");
        return;
    }

    // Obtain pointers to the model's input and output tensors.
    input = interpreter->input(0);
    output = interpreter->output(0);
}

void inference_task(void *pvParameter)
{
    for (;;)
    {
        if (weather.isExist)
        {
            // Quantize the input from floating-point to integer
            uint8_t x_quantized = (uint8_t)(((weather.temperature - 273.15) / input->params.scale) + input->params.zero_point);
            uint8_t y_quantized = (uint8_t)((weather.humidity / input->params.scale) + input->params.zero_point);
            //  Place the quantized input in the model's input tensor
            input->data.uint8[0] = x_quantized;
            input->data.uint8[1] = y_quantized;

            // Run inference, and report any error
            TfLiteStatus invoke_status = interpreter->Invoke();
            if (invoke_status != kTfLiteOk)
            {
                ESP_LOGE(TAG, "Invoke failed \n");
                return;
            }

            // Obtain the quantized output from model's output tensor
            uint8_t out_quantized = output->data.uint8[0];
            // Dequantize the output from integer to floating-point
            // int out = (int)round((out_quantized - output->params.zero_point) * output->params.scale);
            int heat_index = (int)round((out_quantized - output->params.zero_point) * output->params.scale);

            // Output the results. A custom HandleOutput function can be implemented
            // for each supported hardware target.
            // HandleOutput(error_reporter, x, y);
            ESP_LOGI(TAG,
                     "humidity: %d, temperature: %0.2f -> Predict Heat Index: %d \n",
                     weather.humidity, (weather.temperature - 273.15), heat_index);

            weather.isExist = false;
        }
        else
        {
            ESP_LOGD(TAG, "There are no or waiting data. !!!");
        }

        vTaskDelay(TASK_PERIOD / portTICK_PERIOD_MS);
    }
}

void setupHEAT()
{
    configure_tflite();

    xTaskCreate(&inference_task, "inference_task", 20000, NULL, 1, NULL);
}
