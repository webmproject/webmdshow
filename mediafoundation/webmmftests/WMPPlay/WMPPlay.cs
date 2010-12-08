using AxWMPLib;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using WMPLib;

namespace WMPPlay
{
    public partial class WMPPlay : Form
    {
        string webm_file_;

        public WMPPlay(string webm_file)
        {
            webm_file_ = webm_file;
            InitializeComponent();
        }

        private void WMPPlay_Load(object sender, EventArgs e)
        {
            wmp_.URL = webm_file_;
            wmp_.Ctlcontrols.play();
        }

        private void wmp_ErrorEvent(object sender, EventArgs e)
        {
            string err = wmp_.Error.get_Item(0).errorDescription;

            // Display the error description.
            MessageBox.Show(err);
        }

        private void wmp_MediaError(object sender,
                                    _WMPOCXEvents_MediaErrorEvent e)
        {
            try
            // If the Player encounters a corrupt or missing file,
            // show the hexadecimal error code and URL.
            {
                IWMPMedia2 errSource = e.pMediaObject as IWMPMedia2;
                IWMPErrorItem errorItem = errSource.Error;
                MessageBox.Show("Error " + errorItem.errorCode.ToString("X")
                                + " in " + errSource.sourceURL);
            }
            catch (InvalidCastException)
            // In case pMediaObject is not an IWMPMedia item.
            {
                MessageBox.Show("Error.");
            }
        }

        private void wmp_PlayStateChange(object sender,
                                         _WMPOCXEvents_PlayStateChangeEvent e)
        {

            if (e.newState == (int)WMPLib.WMPPlayState.wmppsMediaEnded ||
                e.newState == (int)WMPLib.WMPPlayState.wmppsStopped)
            {
                this.Close();
            }
        }
    }
}
