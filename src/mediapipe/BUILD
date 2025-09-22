cc_library(
    name = "hand_tracker_lib",
    srcs = ["hand_tracker.cpp"],
    hdrs = ["hand_tracker.h"],
    deps = [
        "//mediapipe/framework:calculator_graph",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_highgui",
        "//mediapipe/graphs/hand_tracking:desktop_tflite_calculators",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "net_client_lib",
    srcs = ["net_client.cpp"],
    hdrs = ["net_client.h"],
    # POSIX 소켓만 쓰므로 추가 deps 없음
)

cc_binary(
    name = "hand_palm_demo",
    srcs = ["main.cpp"],
    deps = [
        ":hand_tracker_lib",
        ":net_client_lib",
    ],
    data = [
        "//mediapipe/modules/palm_detection:palm_detection_full.tflite",
        "//mediapipe/modules/hand_landmark:hand_landmark_full.tflite",
        "//mediapipe/modules/hand_landmark:handedness.txt",
    ],
    copts = ["-DMEDIAPIPE_DISABLE_GPU"],
    visibility = ["//visibility:public"],
)
