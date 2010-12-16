// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

using System;
using System.Collections.Generic;

using System.Reflection;
using System.Diagnostics;
using System.Runtime.InteropServices.ComTypes;
using System.Runtime.InteropServices;

using DirectShowLib;
using DirectShowLib.DMO;

using VP8EncoderLib;

namespace WebmTranscode
{
    //WebM Mux filter
    [ComImport, Guid("ED3110F0-5211-11DF-94AF-0026B977EEAA")]
    public class WebMMuxer
    {
    }

    [ComImport, Guid("5C94FE86-B93B-467F-BFC3-BD6C91416F9B")]
    public class VorbisEncoder
    {
    }

    class WebMFilterGraph
    {
        //Field
        private int return_hr;

        //graph builder interfaces
        private IGraphBuilder graph_builder;

        private ICaptureGraphBuilder2 capture_graph_builder2;

        private IMediaControl media_control;
        private IMediaEvent media_event;

        private string input_file;
        private string output_file;

        private int target_bitrate = 0;

        public WebMFilterGraph()
        {
        }

        ~WebMFilterGraph()
        {
        }

        public int ReturnHR
        {
            get
            {
                return return_hr;
            }
            set
            {
                if (value < 0)
                {
                    ShowError();
                    DsError.ThrowExceptionForHR(value);
                }
                else
                {
                    return_hr = value;
                }
            }
        }

        public void LogError(string error_msg)
        {
            Console.WriteLine("Error: {0}", error_msg);
            ShowError();
        }

        private void ShowError()
        {
            StackFrame stack_frame = new StackFrame(1, true);
            MethodBase method_base = stack_frame.GetMethod();
            Console.WriteLine("Method name: {1}, Error Line: {0}", stack_frame.GetFileLineNumber(), method_base.Name);
        }

        public IGraphBuilder GraphBuilder
        {
            get
            {
                return graph_builder;
            }
            set
            {
                graph_builder = value;
            }
        }

        public ICaptureGraphBuilder2 CaptureGraphBuilder2
        {
            get
            {
                return capture_graph_builder2;
            }

            set
            {
                capture_graph_builder2 = value;
            }
        }

        public string InputFile
        {
            get
            {
                return input_file;
            }
            set
            {
                input_file = value;
            }
        }

        public string OutputFile
        {
            get
            {
                return output_file;
            }
            set
            {
                output_file = value;
            }
        }

        public IMediaControl MediaControl
        {
            get
            {
                return media_control;
            }
            set
            {
                media_control = value;
            }
        }

        public IMediaEvent MediaEvent
        {
            get
            {
                return media_event;
            }
            set
            {
                media_event = value;
            }
        }

        public void ParseArguments(string[] args)
        {
            if (args.Length == 1)
            {
                InputFile = args[0];
            }
            else
            {
                InputFile = "sample.wmv"; // default input file
            }

            if (args.Length == 2)
            {
                OutputFile = args[1];
            }
            else
            {
                OutputFile = "sample.webm"; // default output file 
            }

            if (args.Length == 4 && args[2].CompareTo("--target-bitrate") == 0)
            {
                target_bitrate = Int32.Parse(args[3]);
            }

            return;
        }

        public void SetupGraph()
        {
            GraphBuilder = (IGraphBuilder)new FilterGraph();

            // Get ICaptureGraphBuilder2 to build the graph
            CaptureGraphBuilder2 = new CaptureGraphBuilder2() as ICaptureGraphBuilder2;

            try
            {
                ReturnHR = CaptureGraphBuilder2.SetFiltergraph(GraphBuilder);

                IBaseFilter source_filter = null;

                ReturnHR = GraphBuilder.AddSourceFilter(InputFile, "Source Filter", out source_filter);

                IBaseFilter wmv_video_decoder_dmo = (IBaseFilter)new DMOWrapperFilter();
                IDMOWrapperFilter dmo_wrapper_filter_v = (IDMOWrapperFilter)wmv_video_decoder_dmo;

                ReturnHR = dmo_wrapper_filter_v.Init(new Guid("{82D353DF-90BD-4382-8BC2-3F6192B76E34}"), DMOCategory.VideoDecoder);
                ReturnHR = GraphBuilder.AddFilter(wmv_video_decoder_dmo, "Wmv Video Decoder DMO");

                IBaseFilter wmv_audio_decoder_dmo = (IBaseFilter)new DMOWrapperFilter();
                IDMOWrapperFilter dmo_wrapper_filter_a = (IDMOWrapperFilter)wmv_audio_decoder_dmo;

                ReturnHR = dmo_wrapper_filter_a.Init(new Guid("{2EEB4ADF-4578-4D10-BCA7-BB955F56320A}"), DMOCategory.AudioDecoder);
                ReturnHR = GraphBuilder.AddFilter(wmv_audio_decoder_dmo, "Wmv Audio Decoder DMO");

                IBaseFilter vp8_encoder = (IBaseFilter)new VP8Encoder();
                ReturnHR = GraphBuilder.AddFilter(vp8_encoder, "VP8 Encoder");

                IVP8Encoder vp8_encoder_interface = (IVP8Encoder)vp8_encoder;
                if (target_bitrate != 0)
                    vp8_encoder_interface.SetTargetBitrate(target_bitrate);

                IBaseFilter webm_muxer = (IBaseFilter)new WebMMuxer();
                ReturnHR = GraphBuilder.AddFilter(webm_muxer, "WebM Muxer");

                IBaseFilter vorbis_encoder = (IBaseFilter)new VorbisEncoder();
                ReturnHR = GraphBuilder.AddFilter(vorbis_encoder, "Vorbis Encoder");

                IBaseFilter file_writer = (IBaseFilter)new FileWriter();
                ReturnHR = GraphBuilder.AddFilter(file_writer, "file writer");
                IFileSinkFilter filewriter_sink = file_writer as IFileSinkFilter;
                ReturnHR = filewriter_sink.SetFileName(OutputFile, null);

                ReturnHR = GraphBuilder.ConnectDirect(FindPin("Raw Video 1", source_filter), FindPin("in0", wmv_video_decoder_dmo), null);
                ReturnHR = GraphBuilder.ConnectDirect(FindPin("Raw Audio 0", source_filter), FindPin("in0", wmv_audio_decoder_dmo), null);
                ReturnHR = GraphBuilder.ConnectDirect(FindPin("out0", wmv_audio_decoder_dmo), FindPin("PCM In", vorbis_encoder), null);
                ReturnHR = GraphBuilder.ConnectDirect(FindPin("out0", wmv_video_decoder_dmo), FindPin("YUV", vp8_encoder), null);
                ReturnHR = GraphBuilder.ConnectDirect(FindPin("Vorbis Out", vorbis_encoder), FindPin("audio", webm_muxer), null);
                ReturnHR = GraphBuilder.ConnectDirect(FindPin("VP80", vp8_encoder), FindPin("video", webm_muxer), null);
                ReturnHR = GraphBuilder.ConnectDirect(FindPin("outpin", webm_muxer), FindPin("in", file_writer), null);
            }
            catch (Exception)
            {
                LogError("Failed to initialize FilterGraph.");
            }
            finally
            {
                if (CaptureGraphBuilder2 != null)
                {
                    Marshal.ReleaseComObject(CaptureGraphBuilder2);
                    CaptureGraphBuilder2 = null;
                }
            }
        }

        private IPin FindPin(string pin_name, IBaseFilter filter)
        {
            IEnumPins enum_pins;

            ReturnHR = filter.EnumPins(out enum_pins);

            IPin[] pin = new IPin[1];

            while (enum_pins.Next(1, pin, IntPtr.Zero) == 0)
            {
                PinInfo pin_info;
                pin[0].QueryPinInfo(out pin_info);

                if (String.Compare(pin_info.name, pin_name) == 0)
                {
                    DsUtils.FreePinInfo(pin_info);
                    return pin[0];
                }
            }
            LogError("Pin not found");
            return null;
        }

        public void Run()
        {
            media_control = GraphBuilder as IMediaControl;

            media_event = GraphBuilder as IMediaEvent;

            Console.WriteLine("Started filter graph");
            media_control.Run();

            bool stop = false;

            while (!stop)
            {
                System.Threading.Thread.Sleep(500);
                Console.Write(".");
                EventCode ev;
                IntPtr p1, p2;
                if (media_event.GetEvent(out ev, out p1, out p2, 0) == 0)
                {
                    if (ev == EventCode.Complete || ev == EventCode.UserAbort)
                    {
                        Console.WriteLine("Done");
                        stop = true;
                        media_control.Stop();
                    }
                    else if (ev == EventCode.ErrorAbort)
                    {
                        Console.WriteLine("An Error occurred: HRESULT={0:X}", p1);
                        media_control.Stop();
                        stop = true;
                    }
                    media_event.FreeEventParams(ev, p1, p2);
                }
            }
        }
    }

    class WebmTranscode
    {
        static void Main(string[] args)
        {
            if (args.Length == 0)
            {
                ShowUsage();
                return;
            }

            WebMFilterGraph webm_graph = new WebMFilterGraph();

            webm_graph.ParseArguments(args);
            webm_graph.SetupGraph();
            webm_graph.Run();
        }

        static void ShowUsage()
        {
            System.Console.WriteLine("\t\t WebM Transcode Sample for .NET");
            System.Console.WriteLine("\t\tUsage:                      ");
            System.Console.WriteLine("\t\t  WebMTranscode.exe [input file] [output file] <opts> ");
            System.Console.WriteLine("\t\tEx:");
            System.Console.WriteLine("\t\t  WebMTranscode.exe sample.wmv sample.webm --target-bitrate 3000");
        }
    }
}
