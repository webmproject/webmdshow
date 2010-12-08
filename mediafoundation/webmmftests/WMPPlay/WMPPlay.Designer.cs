namespace WMPPlay
{
    partial class WMPPlay
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(WMPPlay));
            this.wmp_ = new AxWMPLib.AxWindowsMediaPlayer();
            ((System.ComponentModel.ISupportInitialize)(this.wmp_)).BeginInit();
            this.SuspendLayout();
            // 
            // wmp_
            // 
            this.wmp_.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
                        | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.wmp_.Enabled = true;
            this.wmp_.Location = new System.Drawing.Point(0, 0);
            this.wmp_.Name = "wmp_";
            this.wmp_.OcxState = ((System.Windows.Forms.AxHost.State)(resources.GetObject("wmp_.OcxState")));
            this.wmp_.Size = new System.Drawing.Size(482, 377);
            this.wmp_.TabIndex = 0;
            this.wmp_.PlayStateChange += new AxWMPLib._WMPOCXEvents_PlayStateChangeEventHandler(this.wmp_PlayStateChange);
            this.wmp_.ErrorEvent += new System.EventHandler(this.wmp_ErrorEvent);
            this.wmp_.MediaError += new AxWMPLib._WMPOCXEvents_MediaErrorEventHandler(this.wmp_MediaError);
            // 
            // WMPPlay
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(480, 376);
            this.Controls.Add(this.wmp_);
            this.Name = "WMPPlay";
            this.Text = "WMPPlay";
            this.Load += new System.EventHandler(this.WMPPlay_Load);
            ((System.ComponentModel.ISupportInitialize)(this.wmp_)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private AxWMPLib.AxWindowsMediaPlayer wmp_;


    }
}

