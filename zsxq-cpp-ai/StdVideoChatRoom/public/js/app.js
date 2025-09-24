/**
 * 主应用类
 * 管理WebSocket连接、UI交互和WebRTC协调
 */
class ChatApp {
    constructor() {
        this.ws = null;
        this.webrtc = new WebRTCManager();
        this.currentRoom = null;
        this.currentUserId = null;
        this.currentUserName = null;
        this.isMobile = this.detectMobile();
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.isLeaving = false;
        this.typingTimer = null;
        this.typingUsers = new Set();
        this.currentTab = 'messages'; // 当前激活的标签页
        this.uploadQueue = []; // 文件上传队列
        this.isUploading = false; // 上传状态
        
        // 根据当前协议确定WebSocket协议
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        this.wsUrl = `${wsProtocol}//${window.location.hostname}:9443`;
        
        // 移动端显示提示
        if (this.isMobile) {
            this.showMobileTips();
        }
        
        // 全屏视频状态
        this.isFullscreen = false;
        this.fullscreenVideoId = null;
        this.fullscreenUserName = null;
    }

    /**
     * 检测是否为移动设备
     * @returns {boolean}
     */
    detectMobile() {
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
    }

    /**
     * 显示移动端使用提示
     */
    showMobileTips() {
        
        // 添加移动端提示信息
        const existingTip = document.getElementById('mobileTip');
        if (!existingTip) {
            const tip = document.createElement('div');
            tip.id = 'mobileTip';
            tip.className = 'mobile-tip';
            tip.innerHTML = `
                <div class="tip-content">
                    📱 移动端提示：
                    <br>• 如果视频无法播放，请点击视频区域
                    <br>• 建议使用WiFi网络以获得更好体验
                    <br>• 如遇问题请尝试刷新页面
                </div>
            `;
            document.body.appendChild(tip);
            
            // 5秒后自动隐藏
            setTimeout(() => {
                tip.style.display = 'none';
            }, 5000);
        }
    }

    /**
     * 连接WebSocket服务器
     */
    connectWebSocket() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.wsUrl);
            
            this.ws.onopen = () => {
                console.log('WebSocket连接成功');
                this.reconnectAttempts = 0; // 重置重连次数
                resolve();
            };
            
            this.ws.onerror = (error) => {
                console.error('WebSocket连接错误:', error);
                reject(error);
            };
            
            this.ws.onclose = (event) => {
                console.log('WebSocket连接关闭, code:', event.code);
                if (!this.isLeaving && this.currentRoom && this.reconnectAttempts < this.maxReconnectAttempts) {
                    this.attemptReconnect();
                } else if (!this.isLeaving && this.currentRoom) {
                    alert('与服务器的连接已断开，请刷新页面重试');
                }
            };
            
            this.ws.onmessage = (event) => {
                this.handleMessage(JSON.parse(event.data));
            };
        });
    }

    /**
     * 尝试重新连接
     */
    async attemptReconnect() {
        this.reconnectAttempts++;
        console.log(`尝试重连 (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
        
        this.showStatus(`连接断开，正在重连... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
        
        try {
            // 清理旧连接状态
            if (this.ws) {
                this.ws.close();
                this.ws = null;
            }
            
            // 等待一段时间再重连，避免服务器清理延迟
            const delay = Math.min(2000 * this.reconnectAttempts, 10000); // 递增延迟，最大10秒
            await new Promise(resolve => setTimeout(resolve, delay));
            
            await this.connectWebSocket();
            
            // 重新加入房间
            if (this.currentRoom) {
                console.log('重新加入房间:', this.currentRoom);
                this.sendMessage({
                    type: 'join_room',
                    roomId: this.currentRoom
                });
            }
            
            this.showStatus('重连成功！', 'success');
        } catch (error) {
            console.error('重连失败:', error);
            if (this.reconnectAttempts >= this.maxReconnectAttempts) {
                this.showStatus('重连失败，请刷新页面重试', 'error');
            } else {
                // 继续尝试重连
                setTimeout(() => this.attemptReconnect(), 1000);
            }
        }
    }

    /**
     * 显示状态信息
     * @param {string} message - 状态消息
     * @param {string} type - 消息类型 (success, error, info)
     */
    showStatus(message, type = 'info') {
        let statusElement = document.getElementById('statusMessage');
        if (!statusElement) {
            statusElement = document.createElement('div');
            statusElement.id = 'statusMessage';
            statusElement.className = 'status-message';
            document.body.appendChild(statusElement);
        }
        
        statusElement.textContent = message;
        statusElement.className = `status-message status-${type}`;
        statusElement.style.display = 'block';
        
        // 成功消息2秒后自动隐藏
        if (type === 'success') {
            setTimeout(() => {
                statusElement.style.display = 'none';
            }, 2000);
        }
    }

    /**
     * 发送WebSocket消息
     * @param {Object} data - 要发送的数据
     */
    sendMessage(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(data));
        } else {
            console.warn('WebSocket未连接，消息发送失败');
        }
    }

    /**
     * 处理接收到的消息
     * @param {Object} data - 接收到的数据
     */
    async handleMessage(data) {
        
        switch (data.type) {
            case 'room_users':
                // 加入房间成功，收到用户列表
                this.currentUserId = data.userId;
                this.currentUserName = data.userName;
                this.updateUI(data.users);
                
                // 延迟一下再建立连接，确保UI更新完成
                setTimeout(async () => {
                    // 向现有用户发起连接
                    for (const user of data.users) {
                        await this.initiateConnection(user.userId);
                    }
                }, 1000);
                break;
                
            case 'user_joined':
                // 新用户加入
                this.addUser(data.userId, data.userName);
                this.showStatus(`${data.userName} 加入了房间`, 'info');
                break;
                
            case 'user_left':
                // 用户离开
                this.removeUser(data.userId);
                this.webrtc.removePeerConnection(data.userId);
                
                // 查找用户名
                const userName = document.querySelector(`#userList li[data-user-id="${data.userId}"]`)?.textContent;
                this.showStatus(`${userName || '用户'} 离开了房间`, 'info');
                break;
                
            case 'user_disconnected':
                // 用户意外断开连接
                this.removeUser(data.userId);
                this.webrtc.removePeerConnection(data.userId);
                
                // 查找用户名
                const disconnectedUserName = document.querySelector(`#userList li[data-user-id="${data.userId}"]`)?.textContent;
                this.showStatus(`${disconnectedUserName || '用户'} 意外断开连接`, 'error');
                break;
                
            case 'offer':
                // 收到Offer
                await this.handleOffer(data);
                break;
                
            case 'answer':
                // 收到Answer
                await this.webrtc.handleAnswer(data.userId, data.answer);
                break;
                
            case 'ice_candidate':
                // 收到ICE候选
                await this.webrtc.addIceCandidate(data.userId, data.candidate);
                break;
                
            case 'error':
                this.showStatus('错误: ' + data.message, 'error');
                break;
                
            case 'text_message':
                // 收到文字消息
                this.handleTextMessage(data);
                break;
                
            case 'message_history':
                // 收到消息历史
                this.handleMessageHistory(data.messages);
                break;
                
            case 'typing_start':
                // 用户开始输入
                this.handleTypingStart(data.userId);
                break;
                
            case 'typing_end':
                // 用户停止输入
                this.handleTypingEnd(data.userId);
                break;
                
            case 'file_uploaded':
                // 收到文件上传通知
                this.handleFileUploaded(data);
                break;
                
            case 'file_list':
                // 收到文件列表
                this.handleFileList(data.files);
                break;
        }
    }

    /**
     * 发起WebRTC连接
     * @param {string} targetUserId - 目标用户ID
     */
    async initiateConnection(targetUserId) {
        console.log(`发起连接到用户: ${targetUserId}`);
        
        // 创建PeerConnection并设置ICE候选回调
        this.webrtc.createPeerConnection(targetUserId, (candidate) => {
            this.sendMessage({
                type: 'ice_candidate',
                targetUserId: targetUserId,
                userId: this.currentUserId,
                candidate: candidate
            });
        });
        
        // 创建并发送Offer
        try {
            const offer = await this.webrtc.createOffer(targetUserId);
            this.sendMessage({
                type: 'offer',
                targetUserId: targetUserId,
                userId: this.currentUserId,
                offer: offer
            });
        } catch (error) {
            console.error('发起连接失败:', error);
            this.showStatus('建立连接失败，请重试', 'error');
        }
    }

    /**
     * 处理收到的Offer
     * @param {Object} data - 包含Offer的数据
     */
    async handleOffer(data) {
        console.log(`收到用户 ${data.userId} 的 Offer`);
        
        // 创建PeerConnection并设置ICE候选回调
        this.webrtc.createPeerConnection(data.userId, (candidate) => {
            this.sendMessage({
                type: 'ice_candidate',
                targetUserId: data.userId,
                userId: this.currentUserId,
                candidate: candidate
            });
        });
        
        // 创建并发送Answer
        try {
            const answer = await this.webrtc.createAnswer(data.userId, data.offer);
            this.sendMessage({
                type: 'answer',
                targetUserId: data.userId,
                userId: this.currentUserId,
                answer: answer
            });
        } catch (error) {
            console.error('处理Offer失败:', error);
            this.showStatus('响应连接失败', 'error');
        }
    }
    
    /**
     * 放大视频显示
     * @param {string} videoId - 视频元素ID
     * @param {string} userName - 用户名
     */
    expandVideo(videoId, userName) {
        if (this.isFullscreen) {
            return; // 已经在全屏模式
        }
        
        const originalVideo = document.getElementById(videoId);
        if (!originalVideo) {
            console.error('找不到视频元素:', videoId);
            return;
        }
        
        // 记录状态
        this.isFullscreen = true;
        this.fullscreenVideoId = videoId;
        this.fullscreenUserName = userName;
        
        // 获取全屏容器和视频元素
        const fullscreenContainer = document.getElementById('fullscreenVideoContainer');
        const fullscreenVideo = document.getElementById('fullscreenVideo');
        const fullscreenLabel = document.getElementById('fullscreenLabel');
        
        if (!fullscreenContainer || !fullscreenVideo || !fullscreenLabel) {
            console.error('找不到全屏元素');
            return;
        }
        
        // 设置全屏视频源
        if (originalVideo.srcObject) {
            fullscreenVideo.srcObject = originalVideo.srcObject;
        } else if (originalVideo.src) {
            fullscreenVideo.src = originalVideo.src;
        }
        
        // 复制视频属性
        fullscreenVideo.muted = originalVideo.muted;
        fullscreenVideo.autoplay = true;
        fullscreenVideo.playsInline = true;
        
        // 设置用户名
        fullscreenLabel.textContent = userName;
        
        // 显示全屏容器
        fullscreenContainer.style.display = 'flex';
        
        // 加上移动端操作支持
        if (this.isMobile) {
            fullscreenVideo.addEventListener('click', () => {
                this.exitFullscreenVideo();
            });
        }
    }
    
    /**
     * 退出全屏视频显示
     */
    exitFullscreenVideo() {
        if (!this.isFullscreen) {
            return;
        }
        
        // 重置状态
        this.isFullscreen = false;
        this.fullscreenVideoId = null;
        this.fullscreenUserName = null;
        
        // 隐藏全屏容器
        const fullscreenContainer = document.getElementById('fullscreenVideoContainer');
        const fullscreenVideo = document.getElementById('fullscreenVideo');
        
        if (fullscreenContainer) {
            fullscreenContainer.style.display = 'none';
        }
        
        // 清空全屏视频源
        if (fullscreenVideo) {
            fullscreenVideo.srcObject = null;
            fullscreenVideo.src = '';
        }
    }

    /**
     * 更新UI显示
     * @param {Array} users - 用户列表
     */
    updateUI(users) {
        // 显示聊天面板
        document.getElementById('loginPanel').style.display = 'none';
        document.getElementById('chatPanel').style.display = 'flex';
        
        // 更新房间信息
        document.getElementById('currentRoomId').textContent = this.currentRoom;
        document.getElementById('currentUserName').textContent = this.currentUserName;
        
        // 更新用户列表
        this.updateUserList(users);
        
        // 移动端额外提示
        if (this.isMobile && users.length > 0) {
            this.showStatus('正在建立视频连接...', 'info');
        }
        
        // 加载文件列表
        this.loadFileList();
        
        // 设置全局引用
        window.chatApp = this;
    }

    /**
     * 更新用户列表显示
     * @param {Array} users - 其他用户列表
     */
    updateUserList(users = []) {
        const userList = document.getElementById('userList');
        const userCount = document.getElementById('userCount');
        userList.innerHTML = '';
        
        // 创建当前用户项
        const selfItem = document.createElement('li');
        selfItem.className = 'current-user';
        selfItem.setAttribute('data-user-id', this.currentUserId);
        selfItem.innerHTML = `
            <span class="user-online-indicator"></span>
            <span class="user-name">${this.currentUserName} (我)</span>
            <span class="user-status">主持人</span>
        `;
        userList.appendChild(selfItem);
        
        // 添加其他用户
        users.forEach(user => {
            this.addUserToList(user.userId, user.userName);
        });
        
        // 更新用户计数
        const totalUsers = users.length + 1; // +1 包括自己
        userCount.textContent = totalUsers;
    }
    
    /**
     * 添加用户到列表
     * @param {string} userId - 用户ID
     * @param {string} userName - 用户名
     */
    addUser(userId, userName) {
        this.addUserToList(userId, userName);
        this.updateUserCount();
    }
    
    /**
     * 添加用户到列表（内部方法）
     * @param {string} userId - 用户ID
     * @param {string} userName - 用户名
     */
    addUserToList(userId, userName) {
        const userList = document.getElementById('userList');
        
        // 检查用户是否已存在
        if (document.querySelector(`#userList li[data-user-id="${userId}"]`)) {
            return;
        }
        
        const userItem = document.createElement('li');
        userItem.setAttribute('data-user-id', userId);
        userItem.innerHTML = `
            <span class="user-online-indicator"></span>
            <span class="user-name">${userName}</span>
            <span class="user-status">在线</span>
        `;
        userList.appendChild(userItem);
    }

    /**
     * 从列表移除用户
     * @param {string} userId - 用户ID
     */
    removeUser(userId) {
        const userItem = document.querySelector(`#userList li[data-user-id="${userId}"]`);
        if (userItem) {
            userItem.remove();
            this.updateUserCount();
        }
    }
    
    /**
     * 更新用户计数
     */
    updateUserCount() {
        const userCount = document.getElementById('userCount');
        const userItems = document.querySelectorAll('#userList li');
        userCount.textContent = userItems.length;
    }

    /**
     * 处理收到的文字消息
     * @param {Object} data - 消息数据
     */
    handleTextMessage(data) {
        this.addChatMessage(data.messageId, data.userId, data.userName, data.content, data.timestamp);
    }

    /**
     * 处理消息历史
     * @param {Array} messages - 消息列表
     */
    handleMessageHistory(messages) {
        const chatMessages = document.getElementById('chatMessages');
        chatMessages.innerHTML = ''; // 清空当前消息
        
        messages.forEach(msg => {
            this.addChatMessage(msg.messageId, msg.userId, msg.userName, msg.content, msg.timestamp, false);
        });
        
        // 滚动到底部
        this.scrollToBottom();
    }

    /**
     * 添加聊天消息到界面
     * @param {string} messageId - 消息ID
     * @param {string} userId - 发送者ID
     * @param {string} userName - 发送者用户名
     * @param {string} content - 消息内容
     * @param {number} timestamp - 时间戳
     * @param {boolean} scroll - 是否滚动到底部
     */
    addChatMessage(messageId, userId, userName, content, timestamp, scroll = true) {
        const chatMessages = document.getElementById('chatMessages');
        
        const messageDiv = document.createElement('div');
        messageDiv.className = `chat-message ${userId === this.currentUserId ? 'own' : 'other'}`;
        messageDiv.setAttribute('data-message-id', messageId);
        
        const messageContent = document.createElement('div');
        messageContent.className = 'message-content';
        messageContent.textContent = content;
        
        const messageInfo = document.createElement('div');
        messageInfo.className = 'message-info';
        
        const time = new Date(timestamp).toLocaleTimeString('zh-CN', { 
            hour: '2-digit', 
            minute: '2-digit' 
        });
        
        if (userId === this.currentUserId) {
            messageInfo.innerHTML = `<span>${time}</span>`;
        } else {
            messageInfo.innerHTML = `<span>${userName}</span><span>${time}</span>`;
        }
        
        messageDiv.appendChild(messageContent);
        messageDiv.appendChild(messageInfo);
        chatMessages.appendChild(messageDiv);
        
        if (scroll) {
            this.scrollToBottom();
        }
    }

    /**
     * 处理用户开始输入
     * @param {string} userId - 用户ID
     */
    handleTypingStart(userId) {
        if (userId !== this.currentUserId) {
            this.typingUsers.add(userId);
            this.updateTypingIndicator();
        }
    }

    /**
     * 处理用户停止输入
     * @param {string} userId - 用户ID
     */
    handleTypingEnd(userId) {
        this.typingUsers.delete(userId);
        this.updateTypingIndicator();
    }

    /**
     * 更新输入指示器
     */
    updateTypingIndicator() {
        const indicator = document.getElementById('typingIndicator');
        
        if (this.typingUsers.size === 0) {
            indicator.style.display = 'none';
            return;
        }
        
        const typingUserNames = [];
        this.typingUsers.forEach(userId => {
            const userElement = document.querySelector(`#userList li[data-user-id="${userId}"]`);
            if (userElement) {
                typingUserNames.push(userElement.textContent);
            }
        });
        
        let text = '';
        if (typingUserNames.length === 1) {
            text = `${typingUserNames[0]} 正在输入...`;
        } else if (typingUserNames.length > 1) {
            text = `${typingUserNames.join(', ')} 正在输入...`;
        }
        
        indicator.querySelector('span').textContent = text;
        indicator.style.display = text ? 'block' : 'none';
        
        if (text) {
            this.scrollToBottom();
        }
    }

    /**
     * 滚动消息到底部
     */
    scrollToBottom() {
        const chatMessages = document.getElementById('chatMessages');
        chatMessages.scrollTop = chatMessages.scrollHeight;
    }

    /**
     * 发送输入状态
     * @param {boolean} isTyping - 是否正在输入
     */
    sendTypingStatus(isTyping) {
        if (!this.currentUserId || !this.currentRoom) return;
        
        this.sendMessage({
            type: isTyping ? 'typing_start' : 'typing_end',
            userId: this.currentUserId,
            roomId: this.currentRoom
        });
    }

    /**
     * 标签页切换
     * @param {string} tabName - 标签名称
     */
    switchTab(tabName) {
        // 更新当前标签状态
        this.currentTab = tabName;
        
        // 更新按钮状态
        const buttons = document.querySelectorAll('.tab-button');
        buttons.forEach(btn => btn.classList.remove('active'));
        
        const activeButton = document.querySelector(`[onclick="switchTab('${tabName}')"]`);
        if (activeButton) {
            activeButton.classList.add('active');
        }
        
        // 显示对应的标签内容
        const contents = document.querySelectorAll('.tab-content');
        contents.forEach(content => content.style.display = 'none');
        
        const targetContent = document.getElementById(tabName + 'Tab');
        if (targetContent) {
            targetContent.style.display = 'flex';
        }
        
        // 如果切换到文件标签，加载文件列表
        if (tabName === 'files') {
            this.loadFileList();
        }
    }

    /**
     * 触发文件选择
     */
    triggerFileUpload() {
        const fileInput = document.getElementById('fileInput');
        if (fileInput) {
            fileInput.click();
        }
    }

    /**
     * 处理文件选择
     * @param {Event} event - 文件选择事件
     */
    handleFileSelect(event) {
        const files = event.target.files;
        if (files && files.length > 0) {
            this.processFiles(Array.from(files));
        }
        // 清空input值，允许重复选择同一文件
        event.target.value = '';
    }

    /**
     * 处理拖拽悬停
     * @param {Event} event - 拖拽事件
     */
    handleDragOver(event) {
        event.preventDefault();
        const uploadZone = document.getElementById('uploadZone');
        if (uploadZone) {
            uploadZone.classList.add('drag-over');
        }
    }

    /**
     * 处理文件拖拽
     * @param {Event} event - 拖拽事件
     */
    handleDrop(event) {
        event.preventDefault();
        const uploadZone = document.getElementById('uploadZone');
        if (uploadZone) {
            uploadZone.classList.remove('drag-over');
        }
        
        const files = event.dataTransfer.files;
        if (files && files.length > 0) {
            this.processFiles(Array.from(files));
        }
    }

    /**
     * 处理文件列表，进行验证和排队上传
     * @param {File[]} files - 文件列表
     */
    processFiles(files) {
        if (!this.currentRoom || !this.currentUserId) {
            this.showStatus('请先加入房间', 'error');
            return;
        }

        // 文件大小限制 (10MB)
        const MAX_FILE_SIZE = 10 * 1024 * 1024;
        
        // 允许的文件类型
        const ALLOWED_TYPES = [
            'image/jpeg', 'image/png', 'image/gif', 'image/webp',
            'text/plain', 'text/csv',
            'application/pdf',
            'application/msword', 'application/vnd.openxmlformats-officedocument.wordprocessingml.document',
            'application/vnd.ms-excel', 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
            'application/vnd.ms-powerpoint', 'application/vnd.openxmlformats-officedocument.presentationml.presentation',
            'application/zip', 'application/x-rar-compressed'
        ];

        const validFiles = [];
        const errors = [];

        for (const file of files) {
            // 检查文件大小
            if (file.size > MAX_FILE_SIZE) {
                errors.push(`文件 "${file.name}" 大小超过限制（最大10MB）`);
                continue;
            }

            // 检查文件类型
            if (!ALLOWED_TYPES.includes(file.type)) {
                errors.push(`文件 "${file.name}" 类型不被支持`);
                continue;
            }

            validFiles.push(file);
        }

        // 显示错误信息
        if (errors.length > 0) {
            this.showStatus(errors.join('\n'), 'error');
        }

        // 添加有效文件到上传队列
        if (validFiles.length > 0) {
            this.uploadQueue.push(...validFiles);
            this.processUploadQueue();
        }
    }

    /**
     * 处理上传队列
     */
    async processUploadQueue() {
        if (this.isUploading || this.uploadQueue.length === 0) {
            return;
        }

        this.isUploading = true;
        
        while (this.uploadQueue.length > 0) {
            const file = this.uploadQueue.shift();
            try {
                await this.uploadFile(file);
            } catch (error) {
                console.error('文件上传失败:', error);
                this.showStatus(`文件 "${file.name}" 上传失败: ${error.message}`, 'error');
            }
            
            // 短暂延迟，避免过快的连续请求
            await new Promise(resolve => setTimeout(resolve, 500));
        }
        
        this.isUploading = false;
    }

    /**
     * 上传单个文件
     * @param {File} file - 要上传的文件
     */
    async uploadFile(file) {
        return new Promise((resolve, reject) => {
            const formData = new FormData();
            formData.append('file', file);

            const xhr = new XMLHttpRequest();
            
            // 显示上传进度
            this.showUploadProgress(file.name, 0);
            
            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percentage = Math.round((e.loaded / e.total) * 100);
                    this.updateUploadProgress(percentage);
                }
            });
            
            xhr.addEventListener('load', () => {
                this.hideUploadProgress();
                
                if (xhr.status === 200) {
                    try {
                        const response = JSON.parse(xhr.responseText);
                        this.showStatus(`文件 "${file.name}" 上传成功`, 'success');
                        
                        // 上传成功后刷新文件列表
                        setTimeout(() => {
                            this.loadFileList();
                        }, 500);
                        
                        resolve(response);
                    } catch (e) {
                        reject(new Error('服务器响应格式错误'));
                    }
                } else {
                    try {
                        const error = JSON.parse(xhr.responseText);
                        reject(new Error(error.error || '上传失败'));
                    } catch (e) {
                        reject(new Error(`上传失败 (${xhr.status})`));
                    }
                }
            });
            
            xhr.addEventListener('error', () => {
                this.hideUploadProgress();
                reject(new Error('网络错误'));
            });
            
            xhr.addEventListener('timeout', () => {
                this.hideUploadProgress();
                reject(new Error('上传超时'));
            });
            
            xhr.timeout = 30000; // 30秒超时
            xhr.open('POST', '/api/upload');
            xhr.setRequestHeader('X-Room-Id', this.currentRoom);
            xhr.setRequestHeader('X-User-Id', this.currentUserId);
            xhr.send(formData);
        });
    }

    /**
     * 显示上传进度
     * @param {string} filename - 文件名
     * @param {number} percentage - 进度百分比
     */
    showUploadProgress(filename, percentage) {
        const progressDialog = document.getElementById('uploadProgress');
        const progressText = progressDialog?.querySelector('.progress-text');
        const progressFill = document.getElementById('progressFill');
        const progressPercentage = document.getElementById('progressPercentage');
        
        if (progressDialog) {
            progressDialog.style.display = 'block';
        }
        if (progressText) {
            progressText.textContent = `正在上传: ${filename}`;
        }
        if (progressFill) {
            progressFill.style.width = `${percentage}%`;
        }
        if (progressPercentage) {
            progressPercentage.textContent = `${percentage}%`;
        }
    }

    /**
     * 更新上传进度
     * @param {number} percentage - 进度百分比
     */
    updateUploadProgress(percentage) {
        const progressFill = document.getElementById('progressFill');
        const progressPercentage = document.getElementById('progressPercentage');
        
        if (progressFill) {
            progressFill.style.width = `${percentage}%`;
        }
        if (progressPercentage) {
            progressPercentage.textContent = `${percentage}%`;
        }
    }

    /**
     * 隐藏上传进度
     */
    hideUploadProgress() {
        const progressDialog = document.getElementById('uploadProgress');
        if (progressDialog) {
            progressDialog.style.display = 'none';
        }
    }

    /**
     * 加载文件列表
     */
    async loadFileList() {
        if (!this.currentRoom) {
            return;
        }

        try {
            const response = await fetch(`/api/files/${this.currentRoom}`, {
                headers: {
                    'X-User-Id': this.currentUserId,
                    'X-Room-Id': this.currentRoom
                }
            });

            if (response.ok) {
                const data = await response.json();
                this.displayFileList(data.files);
            } else {
                console.error('加载文件列表失败:', response.status);
            }
        } catch (error) {
            console.error('加载文件列表出错:', error);
        }
    }

    /**
     * 显示文件列表
     * @param {Array} files - 文件列表
     */
    displayFileList(files) {
        const fileItems = document.getElementById('fileItems');
        if (!fileItems) return;

        if (!files || files.length === 0) {
            fileItems.innerHTML = '<div class="no-files">暂无文件</div>';
            return;
        }

        fileItems.innerHTML = files.map(file => {
            const uploadTime = new Date(file.uploadTime).toLocaleString('zh-CN');
            const fileSize = this.formatFileSize(file.size);
            const fileIcon = this.getFileIcon(file.mimeType);
            
            return `
                <div class="file-item">
                    <div class="file-icon">${fileIcon}</div>
                    <div class="file-info">
                        <div class="file-name" title="${file.filename}">${file.filename}</div>
                        <div class="file-meta">
                            <span>${fileSize}</span>
                            <span>${file.uploaderName}</span>
                            <span>${uploadTime}</span>
                        </div>
                    </div>
                    <div class="file-actions">
                        <button class="file-download-btn" onclick="app.downloadFile('${file.fileId}', '${file.filename}')" title="下载">⬇</button>
                    </div>
                </div>
            `;
        }).join('');
    }

    /**
     * 下载文件
     * @param {string} fileId - 文件ID
     * @param {string} filename - 文件名
     */
    downloadFile(fileId, filename) {
        const link = document.createElement('a');
        link.href = `/api/download/${fileId}`;
        link.download = filename;
        link.click();
    }

    /**
     * 格式化文件大小
     * @param {number} bytes - 字节数
     * @returns {string} - 格式化的大小
     */
    formatFileSize(bytes) {
        if (bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
    }

    /**
     * 获取文件图标
     * @param {string} mimeType - MIME类型
     * @returns {string} - 图标字符
     */
    getFileIcon(mimeType) {
        if (mimeType.startsWith('image/')) return '🖼️';
        if (mimeType.includes('pdf')) return '📄';
        if (mimeType.includes('word') || mimeType.includes('document')) return '📝';
        if (mimeType.includes('excel') || mimeType.includes('spreadsheet')) return '📊';
        if (mimeType.includes('powerpoint') || mimeType.includes('presentation')) return '📊';
        if (mimeType.includes('zip') || mimeType.includes('rar')) return '📦';
        if (mimeType.startsWith('text/')) return '📄';
        return '📁';
    }

    /**
     * 处理文件上传通知
     * @param {Object} data - 文件信息
     */
    handleFileUploaded(data) {
        // 总是刷新文件列表，确保数据最新
        this.loadFileList();
        
        // 显示通知
        this.showStatus(`${data.uploaderUserName || data.uploaderName} 上传了文件: ${data.filename}`, 'info');
    }

    /**
     * 处理文件列表
     * @param {Array} files - 文件列表
     */
    handleFileList(files) {
        this.displayFileList(files);
    }
}

// 全局应用实例
let app = null;

/**
 * 加入房间
 */
async function joinRoom() {
    const roomId = document.getElementById('roomIdInput').value.trim();
    
    if (!roomId) {
        alert('请输入房间号');
        return;
    }
    
    const joinButton = document.getElementById('joinButton');
    joinButton.disabled = true;
    joinButton.textContent = '连接中...';
    
    try {
        // 创建应用实例
        app = new ChatApp();
        app.currentRoom = roomId;
        
        // 初始化本地媒体流
        await app.webrtc.initLocalStream();
        
        // 连接WebSocket
        await app.connectWebSocket();
        
        // 发送加入房间请求
        app.sendMessage({
            type: 'join_room',
            roomId: roomId
        });
        
        // 添加心跳检测，定期发送ping消息检测连接状态
        app.pingInterval = setInterval(() => {
            if (app && app.ws && app.ws.readyState === WebSocket.OPEN) {
                app.sendMessage({
                    type: 'ping',
                    userId: app.currentUserId,
                    roomId: app.currentRoom
                });
            }
        }, 30000); // 每30秒发送一次心跳
        
    } catch (error) {
        console.error('加入房间失败:', error);
        alert('加入房间失败，请检查网络连接和设备权限');
        
        joinButton.disabled = false;
        joinButton.textContent = '加入房间';
    }
}

/**
 * 发送文字消息
 */
function sendMessage() {
    if (!app) return;
    
    const messageInput = document.getElementById('messageInput');
    const sendButton = document.getElementById('sendButton');
    const content = messageInput.value.trim();
    
    if (!content) return;
    
    // 禁用输入和按钮
    messageInput.disabled = true;
    sendButton.disabled = true;
    sendButton.textContent = '发送中...';
    
    // 发送消息
    app.sendMessage({
        type: 'text_message',
        userId: app.currentUserId,
        roomId: app.currentRoom,
        content: content
    });
    
    // 清空输入框
    messageInput.value = '';
    
    // 发送停止输入状态
    if (app.typingTimer) {
        clearTimeout(app.typingTimer);
        app.typingTimer = null;
    }
    app.sendTypingStatus(false);
    
    // 重新启用输入
    setTimeout(() => {
        messageInput.disabled = false;
        sendButton.disabled = false;
        sendButton.textContent = '发送';
        messageInput.focus();
    }, 100);
}

/**
 * 离开房间
 */
function leaveRoom() {
    if (!app) return;
    
    // 设置离开标志位，防止自动重连
    app.isLeaving = true;
    
    // 清除心跳检测
    if (app.pingInterval) {
        clearInterval(app.pingInterval);
        app.pingInterval = null;
    }
    
    // 发送离开房间消息
    app.sendMessage({
        type: 'leave_room',
        roomId: app.currentRoom,
        userId: app.currentUserId
    });
    
    // 清理WebRTC连接
    app.webrtc.cleanup();
    
    // 清空房间信息（在关闭 WebSocket 之前）
    app.currentRoom = null;
    app.currentUserId = null;
    app.currentUserName = null;
    
    // 关闭WebSocket
    if (app.ws) {
        app.ws.close();
        app.ws = null;
    }
    
    // 返回登录界面
    document.getElementById('chatPanel').style.display = 'none';
    document.getElementById('loginPanel').style.display = 'block';
    document.getElementById('roomIdInput').value = '';
    
    // 重置按钮状态
    const joinButton = document.getElementById('joinButton');
    joinButton.disabled = false;
    joinButton.textContent = '加入房间';
    
    // 清空远程视频
    document.getElementById('remoteVideos').innerHTML = '';
    
    // 清空聊天消息
    document.getElementById('chatMessages').innerHTML = '';
    document.getElementById('messageInput').value = '';
    document.getElementById('typingIndicator').style.display = 'none';
    
    // 隐藏状态消息
    const statusElement = document.getElementById('statusMessage');
    if (statusElement) {
        statusElement.style.display = 'none';
    }
    
    app = null;
}

/**
 * 切换静音
 */
function toggleMute() {
    if (!app || !app.webrtc) return;
    
    const isMuted = app.webrtc.toggleMute();
    const button = document.getElementById('muteButton');
    
    if (isMuted) {
        button.classList.add('muted');
        button.querySelector('.icon').textContent = '🔇';
    } else {
        button.classList.remove('muted');
        button.querySelector('.icon').textContent = '🎤';
    }
}

/**
 * 切换摄像头
 */
function toggleCamera() {
    if (!app || !app.webrtc) return;
    
    const isOff = app.webrtc.toggleCamera();
    const button = document.getElementById('cameraButton');
    
    if (isOff) {
        button.classList.add('muted');
        button.querySelector('.icon').textContent = '📵';
    } else {
        button.classList.remove('muted');
        button.querySelector('.icon').textContent = '📷';
    }
}

/**
 * 标签页切换 - 全局函数
 * @param {string} tabName - 标签名称
 */
function switchTab(tabName) {
    if (app) {
        app.switchTab(tabName);
    }
}

/**
 * 触发文件上传 - 全局函数
 */
function triggerFileUpload() {
    if (app) {
        app.triggerFileUpload();
    }
}

/**
 * 处理文件选择 - 全局函数
 * @param {Event} event - 文件选择事件
 */
function handleFileSelect(event) {
    if (app) {
        app.handleFileSelect(event);
    }
}

/**
 * 处理拖拽悬停 - 全局函数
 * @param {Event} event - 拖拽事件
 */
function handleDragOver(event) {
    if (app) {
        app.handleDragOver(event);
    }
}

/**
 * 处理文件拖拽 - 全局函数
 * @param {Event} event - 拖拽事件
 */
function handleDrop(event) {
    if (app) {
        app.handleDrop(event);
    }
}

// 监听Enter键和输入事件
document.addEventListener('DOMContentLoaded', () => {
    // 房间输入框
    document.getElementById('roomIdInput').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            joinRoom();
        }
    });
    
    // 消息输入框
    const messageInput = document.getElementById('messageInput');
    if (messageInput) {
        messageInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                sendMessage();
            }
        });
        
        // 输入状态检测
        messageInput.addEventListener('input', () => {
            if (!app || !app.currentUserId || !app.currentRoom) return;
            
            const content = messageInput.value.trim();
            
            if (content.length > 0) {
                // 开始输入
                if (!app.typingTimer) {
                    app.sendTypingStatus(true);
                }
                
                // 重置停止输入的定时器
                clearTimeout(app.typingTimer);
                app.typingTimer = setTimeout(() => {
                    app.sendTypingStatus(false);
                    app.typingTimer = null;
                }, 1000); // 1秒后发送停止输入
            } else {
                // 输入框为空，立即停止输入状态
                if (app.typingTimer) {
                    clearTimeout(app.typingTimer);
                    app.typingTimer = null;
                    app.sendTypingStatus(false);
                }
            }
        });
        
        // 失去焦点时停止输入状态
        messageInput.addEventListener('blur', () => {
            if (!app) return;
            
            if (app.typingTimer) {
                clearTimeout(app.typingTimer);
                app.typingTimer = null;
                app.sendTypingStatus(false);
            }
        });
    }
});

// 页面关闭时清理资源
window.addEventListener('beforeunload', () => {
    if (app) {
        leaveRoom();
    }
});

// 移动端页面可见性变化处理
document.addEventListener('visibilitychange', () => {
    if (app && app.isMobile) {
        if (!document.hidden) {
            // 页面回到前台时可以添加重新连接逻辑
        }
    }
});

// 全屏视频相关全局函数
function expandVideo(videoId, userName) {
    if (window.chatApp && typeof window.chatApp.expandVideo === 'function') {
        window.chatApp.expandVideo(videoId, userName);
    }
}

function exitFullscreenVideo() {
    if (window.chatApp && typeof window.chatApp.exitFullscreenVideo === 'function') {
        window.chatApp.exitFullscreenVideo();
    }
}

// ESC键退出全屏
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && window.chatApp && window.chatApp.isFullscreen) {
        window.chatApp.exitFullscreenVideo();
    }
});

// 鼠标右键菜单禁用（防止用户意外退出全屏）
document.addEventListener('contextmenu', (e) => {
    if (window.chatApp && window.chatApp.isFullscreen) {
        e.preventDefault();
    }
}); 