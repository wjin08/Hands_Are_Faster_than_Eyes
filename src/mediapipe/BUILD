cc_binary(
    name = "hand_palm_demo",
    srcs = ["hand_tracker.cpp"],
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
	data = [
    "//mediapipe/modules/palm_detection:palm_detection_full.tflite",
    "//mediapipe/modules/hand_landmark:hand_landmark_full.tflite",
    "//mediapipe/modules/hand_landmark:handedness.txt",
	],
    copts = ["-DMEDIAPIPE_DISABLE_GPU"],
    visibility = ["//visibility:public"],
)